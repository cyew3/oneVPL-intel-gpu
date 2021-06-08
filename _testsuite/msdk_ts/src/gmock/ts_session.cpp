/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_session.h"
#include "mfxdispatcher.h"
#include "sample_utils.h"

#if (defined(LINUX32) || defined(LINUX64))

#include <link.h>

#endif

tsSession::tsSession(mfxIMPL impl, mfxVersion version)
    : m_initialized(false)
    , m_sw_fallback(false)
    , m_is_handle_set(false)
    , m_session(0)
    , m_impl(impl)
    , m_version(version)
    , m_syncpoint(0)
    , m_pSession(&m_session)
    , m_pVersion(&m_version)
    , m_pFrameAllocator(0)
    , m_pVAHandle(0)
    , m_init_par()
{
}

tsSession::~tsSession()
{
    if(m_initialized)
    {
        MFXClose();
    }

    ReleaseLoader();

#if (defined(LINUX32) || defined(LINUX64))
    if (m_pVAHandle)
    {
        delete m_pVAHandle;
    }
#endif
}

mfxStatus tsSession::ConfigureImp(mfxIMPL impl)
{
    mfxConfig cfg = MFXCreateConfig(m_Loader);

    mfxVariant variant;
    variant.Type = MFX_VARIANT_TYPE_U32;
    variant.Data.U32 = -1;

    switch (m_impl)
    {
        case MFX_IMPL_SOFTWARE:
            variant.Data.U32 = MFX_IMPL_TYPE_SOFTWARE;
            break;
        case MFX_IMPL_HARDWARE:
        case MFX_IMPL_HARDWARE_ANY:
        case MFX_IMPL_HARDWARE2:
        case MFX_IMPL_HARDWARE3:
        case MFX_IMPL_HARDWARE4:
        case MFX_IMPL_VIA_D3D9:
        case MFX_IMPL_VIA_D3D11:
        case MFX_IMPL_VIA_VAAPI:
            variant.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
            break;
    }

    if (variant.Data.U32 != -1) {
        g_tsStatus.expect(MFX_ERR_NONE);
        g_tsStatus.check(MFXSetConfigFilterProperty(cfg, (mfxU8*)"mfxImplDescription.Impl", variant));
        m_Configs.push_back(cfg);
    }

    return g_tsStatus.get();
}

mfxStatus tsSession::ConfigureApiVersion(mfxVersion *ver)
{
    mfxConfig cfg = MFXCreateConfig(m_Loader);
    mfxVariant variant;
    variant.Type = MFX_VARIANT_TYPE_U32;
    variant.Data.U32 = ver->Version;
    g_tsStatus.expect(MFX_ERR_NONE);
    g_tsStatus.check(MFXSetConfigFilterProperty(cfg, (mfxU8*)"mfxImplDescription.ApiVersion.Version", variant));
    m_Configs.push_back(cfg);

    return g_tsStatus.get();
}

mfxStatus tsSession::CreateSession(mfxIMPL impl, mfxVersion *ver, mfxSession *session)
{
    TRACE_FUNC0( MFXLoad );
    m_Loader = MFXLoad();
    if (!m_Loader) {
        g_tsStatus.check(MFX_ERR_UNKNOWN);
    }

    ConfigureImp(impl);

    if ( ver )
    {
        ConfigureApiVersion(ver);
    }

    int implIndex = 0;

// pick the first available implementation, m_desc is checked only for being non-equal to NULL
// TODO: add loop by implementations, set implIndex corresponding better implementation and add meaningful validation for the m_idesc
    mfxImplDescription  * idesc;
    TRACE_FUNC4( MFXEnumImplementations, m_Loader, implIndex, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL*)&idesc );
    g_tsStatus.expect(MFX_ERR_NONE);
    g_tsStatus.check(MFXEnumImplementations(m_Loader, implIndex, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL*)&idesc));
    if (!idesc) {
        g_tsStatus.check(MFX_ERR_UNKNOWN);
    }

    m_idesc.reset(idesc);

    TRACE_FUNC3( MFXCreateSession, m_Loader, implIndex, session );
    g_tsStatus.expect(MFX_ERR_NONE);
    g_tsStatus.check(MFXCreateSession(m_Loader, implIndex, session));

#if (defined(LINUX32) || defined(LINUX64))
    msdk_printf(MSDK_STRING("Used implementation number: %d \n"), implIndex);
    msdk_printf(MSDK_STRING("Loaded modules:\n"));
    int numLoad = 0;
    dl_iterate_phdr(PrintLibMFXPath, &numLoad);
#else

#if !defined(MFX_DISPATCHER_LOG)
    PrintLoadedModules();
#endif // !defined(MFX_DISPATCHER_LOG)

#endif //(defined(LINUX32) || defined(LINUX64))

    return g_tsStatus.get();
}

mfxStatus tsSession::ReleaseLoader()
{
    if (m_idesc.get())
    {
        g_tsStatus.expect(MFX_ERR_NONE);
        g_tsStatus.check(MFXDispReleaseImplDescription(m_Loader, m_idesc.get()));
        m_idesc.release();
        MFXUnload(m_Loader);
    }

    return g_tsStatus.get();
}

mfxStatus tsSession::MFXInit()
{
    return MFXInit(m_impl, m_pVersion, m_pSession);
}

mfxStatus tsSession::MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session)
{

    if (ver->Major < 2)
    {
        TRACE_FUNC3( MFXInit, impl, ver, session );
        g_tsStatus.check(::MFXInit(impl, ver, session));
    }
    else
    {
       CreateSession(impl, ver, session);
    }

    TS_TRACE(ver);
    TS_TRACE(session);

    m_initialized = (g_tsStatus.get() >= 0);

#if (defined(LINUX32) || defined(LINUX64))
    // SetHandle on Linux is always required for HW
    if (m_initialized && g_tsImpl != MFX_IMPL_SOFTWARE)
    {
        mfxHDL hdl;
        mfxHandleType type;
        if (!m_pVAHandle)
        {
            m_pVAHandle = new frame_allocator(
                    frame_allocator::HARDWARE,
                    frame_allocator::ALLOC_MAX,
                    frame_allocator::ENABLE_ALL,
                    frame_allocator::ALLOC_EMPTY);
        }
        m_pVAHandle->get_hdl(type, hdl);
        SetHandle(m_session, type, hdl);
        m_is_handle_set = (g_tsStatus.get() >= 0);
    }
#endif

    return g_tsStatus.get();
}

mfxStatus tsSession::MFXClose()
{
    return MFXClose(m_session);
}

mfxStatus tsSession::MFXClose(mfxSession session)
{
    TRACE_FUNC1( MFXClose, session );
    g_tsStatus.check(::MFXClose(session));
    m_initialized = false;

    ReleaseLoader();

    return g_tsStatus.get();
}

mfxStatus tsSession::SyncOperation()
{
    return SyncOperation(m_session, m_syncpoint, MFX_INFINITE);
}

mfxStatus tsSession::SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait)
{
    TRACE_FUNC3(MFXVideoCORE_SyncOperation, session, syncp, wait);
    g_tsStatus.check( MFXVideoCORE_SyncOperation(session, syncp, wait) );

    return g_tsStatus.get();
}

mfxStatus tsSession::SetFrameAllocator()
{
    return SetFrameAllocator(m_session, m_pFrameAllocator);
}

mfxStatus tsSession::SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator)
{
    TRACE_FUNC2(MFXVideoCORE_SetFrameAllocator, session, allocator);
    g_tsStatus.check( MFXVideoCORE_SetFrameAllocator(session, allocator) );
    return g_tsStatus.get();
}

mfxStatus tsSession::Load(mfxSession session, const mfxPluginUID *uid, mfxU32 version)
{
    return g_tsStatus.get();
}

mfxStatus tsSession::UnLoad(mfxSession session, const mfxPluginUID *uid)
{
    g_tsLog << "Plugins are not supported by VPL\n";
    throw tsFAIL;
    return g_tsStatus.get();
}

mfxStatus tsSession::SetHandle(mfxSession session, mfxHandleType type, mfxHDL handle)
{
    TRACE_FUNC3(MFXVideoCORE_SetHandle, session, type, handle);
    g_tsStatus.check( MFXVideoCORE_SetHandle(session, type, handle) );
    m_is_handle_set = (g_tsStatus.get() >= 0);
    return g_tsStatus.get();
}

mfxStatus tsSession::GetPlugin(mfxSession session, mfxU32 type, mfxPlugin *plugin)
{
    g_tsLog << "Plugins are not supported by VPL\n";
    throw tsFAIL;
    return g_tsStatus.get();
}

mfxStatus tsSession::MFXQueryVersion(mfxSession session, mfxVersion *version)
{
    TRACE_FUNC2(MFXQueryVersion, session, version);
    g_tsStatus.check( ::MFXQueryVersion(session, version) );
    return g_tsStatus.get();
}

mfxStatus tsSession::MFXInitEx()
{
    return MFXInitEx(m_init_par, &m_session);
}

mfxStatus tsSession::MFXInitEx(mfxInitParam par, mfxSession* session)
{
    if (par.Version.Major < 2)
    {
        TRACE_FUNC2(mfxInitParam, par, session);
        g_tsStatus.check( ::MFXInitEx(par, session) );
    }
    else
    {
       CreateSession(par.Implementation, &par.Version, session);
    }

    TS_TRACE(par);

    m_initialized = (g_tsStatus.get() >= 0);

#if (defined(LINUX32) || defined(LINUX64))
    // SetHandle on Linux is always required for HW
    if (m_initialized && g_tsImpl != MFX_IMPL_SOFTWARE)
    {
        mfxHDL hdl;
        mfxHandleType type;
        if (!m_pVAHandle)
        {
            m_pVAHandle = new frame_allocator(
                    frame_allocator::HARDWARE,
                    frame_allocator::ALLOC_MAX,
                    frame_allocator::ENABLE_ALL,
                    frame_allocator::ALLOC_EMPTY);
        }
        m_pVAHandle->get_hdl(type, hdl);
        SetHandle(m_session, type, hdl);
        m_is_handle_set = (g_tsStatus.get() >= 0);
    }
#endif

    return g_tsStatus.get();
}

mfxStatus tsSession::MFXJoinSession(mfxSession session)
{
    TRACE_FUNC2(MFXJoinSession, m_session, session);
    g_tsStatus.check( ::MFXJoinSession(m_session, session) );
    return g_tsStatus.get();
}

mfxStatus tsSession::MFXDisjoinSession()
{
    TRACE_FUNC1(MFXJoinSession, m_session);
    g_tsStatus.check( ::MFXDisjoinSession(m_session) );
    return g_tsStatus.get();
}
