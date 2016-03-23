/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

File Name: libmfxsw.cpp

\* ****************************************************************************** */

#include <mfxvideo.h>

#include <mfx_session.h>
#include <mfx_check_hardware_support.h>
#include <mfx_trace.h>

#include <ippcore.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <tchar.h>
#endif

#if (MFX_VERSION_MAJOR == 1) && (MFX_VERSION_MINOR >= 10)
  #define MFX_USE_VERSIONED_SESSION
#endif


// static section of the file
namespace
{

void* g_hModule = NULL; // DLL handle received in DllMain

} // namespace

#if !defined(_WIN32) && !defined(_WIN64)

/* These string constants set Media SDK version information for Linux, Android, OSX. */
#ifndef MFX_FILE_VERSION
#define MFX_FILE_VERSION "0.0.0.0"
#endif
#ifndef MFX_PRODUCT_VERSION
#define MFX_PRODUCT_VERSION "0.0.000.0000"
#endif

// Copyright strings
#if defined(mfxhw64_EXPORTS) || defined(mfxhw32_EXPORTS) || defined(mfxsw64_EXPORTS) || defined(mfxsw32_EXPORTS)

const char* g_MfxProductName = "mediasdk_product_name: Intel(R) Media Server Studio 2016 - SDK for Linux*";
const char* g_MfxCopyright = "mediasdk_copyright: Copyright(c) 2007-2016 Intel Corporation";
const char* g_MfxFileVersion = "mediasdk_file_version: " MFX_FILE_VERSION;
const char* g_MfxProductVersion = "mediasdk_product_version: " MFX_PRODUCT_VERSION;

#endif // mfxhwXX_EXPORTS
#if defined(mfxaudiosw64_EXPORTS) || defined(mfxaudiosw32_EXPORTS)
const char* g_MfxProductName = "mediasdk_product_name: Intel(R) Media Server Studio 2016 - Audio for Linux*";
const char* g_MfxCopyright = "mediasdk_copyright: Copyright(c) 2014-2016 Intel Corporation";
const char* g_MfxFileVersion = "mediasdk_file_version: " MFX_FILE_VERSION;
const char* g_MfxProductVersion = "mediasdk_product_version: " MFX_PRODUCT_VERSION;

#endif // mfxaudioswXX_EXPORTS

#endif // Linux

mfxStatus MFXInit(mfxIMPL implParam, mfxVersion *ver, mfxSession *session)
{
    mfxInitParam par = {};

    par.Implementation = implParam;
    if (ver)
    {
        par.Version = *ver;
    }
    else
    {
        par.Version.Major = MFX_VERSION_MAJOR;
        par.Version.Minor = MFX_VERSION_MINOR;
    }
    par.ExternalThreads = 0;

    return MFXInitEx(par, session);

} // mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxHDL *session)

mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session)
{
#if defined(MFX_USE_VERSIONED_SESSION)
    _mfxSession_1_10 * pSession = 0;
#else
    _mfxSession * pSession = 0;
#endif
    mfxVersion libver;
    mfxStatus mfxRes;
    int adapterNum = 0;
    mfxIMPL impl = par.Implementation & (MFX_IMPL_VIA_ANY - 1);
    mfxIMPL implInterface = par.Implementation & -MFX_IMPL_VIA_ANY;

#if defined(MFX_TRACE_ENABLE)
#if defined(_WIN32) || defined(_WIN64)

    #if 1//defined(MFX_VA)
    MFX_TRACE_INIT(NULL, (mfxU8)MFX_TRACE_OUTPUT_REG);
    #endif // #if defined(MFX_VA)

#else

    MFX_TRACE_INIT(NULL, MFX_TRACE_OUTPUT_TRASH);

#endif // #if defined(_WIN32) || defined(_WIN64)
#endif // defined(MFX_TRACE_ENABLE) && defined(MFX_VA)

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "ThreadName=MSDK app");
    }
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXInit");
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API, "^ModuleHandle^libmfx=", "%p", g_hModule);

#if defined (MFX_RT)
    // check error(s)
    if ((MFX_IMPL_AUTO != impl) &&
        (MFX_IMPL_AUTO_ANY != impl) &&
        (MFX_IMPL_HARDWARE_ANY != impl) &&
        (MFX_IMPL_HARDWARE != impl) &&
        (MFX_IMPL_HARDWARE2 != impl) &&
        (MFX_IMPL_HARDWARE3 != impl) &&
        (MFX_IMPL_HARDWARE4 != impl) &&
        (MFX_IMPL_SOFTWARE != impl)) 
    {
        return MFX_ERR_UNSUPPORTED;
    }
#else
        // check error(s)
    if ((MFX_IMPL_AUTO != impl) &&
        (MFX_IMPL_AUTO_ANY != impl) &&
#ifdef MFX_VA
        (MFX_IMPL_HARDWARE_ANY != impl) &&
        (MFX_IMPL_HARDWARE != impl) &&
        (MFX_IMPL_HARDWARE2 != impl) &&
        (MFX_IMPL_HARDWARE3 != impl) &&
        (MFX_IMPL_HARDWARE4 != impl))
#else // !MFX_VA
        (MFX_IMPL_SOFTWARE != impl))
#endif // MFX_VA
    {
        return MFX_ERR_UNSUPPORTED;
    }
#endif


    if (!(implInterface & MFX_IMPL_AUDIO) &&
        !(implInterface & MFX_IMPL_EXTERNAL_THREADING)&&
        (0 != implInterface) &&
#if defined(MFX_VA_WIN)
        (MFX_IMPL_VIA_D3D11 != implInterface) &&
        (MFX_IMPL_VIA_D3D9 != implInterface) &&
#elif defined(MFX_VA_LINUX)
        (MFX_IMPL_VIA_VAAPI != implInterface) &&
#endif // MFX_VA_*
        (MFX_IMPL_VIA_ANY != implInterface))
    {
        return MFX_ERR_UNSUPPORTED;
    }


    // set the adapter number
    switch (impl)
    {
    case MFX_IMPL_HARDWARE2:
        adapterNum = 1;
        break;

    case MFX_IMPL_HARDWARE3:
        adapterNum = 2;
        break;

    case MFX_IMPL_HARDWARE4:
        adapterNum = 3;
        break;

    default:
        adapterNum = 0;
        break;

    }

#ifdef MFX_VA
    // the hardware is not supported,
    // or the vendor is not Intel.
    //if (MFX_HW_UNKNOWN == MFX::GetHardwareType())
    //{
    //    // if it is a hardware library,
    //    // reject everything.
    //    // Do not work under any curcumstances.
    //    return MFX_ERR_UNSUPPORTED;
    //}
#endif // MFX_VA

    try
    {
        // reset output variable
        *session = 0;
        // prepare initialization parameters

        // create new session instance
#if defined(MFX_USE_VERSIONED_SESSION)
        pSession = new _mfxSession_1_10(adapterNum);
#else
        pSession = new _mfxSession(adapterNum);
#endif
        mfxInitParam init_param = par;
        init_param.Implementation = implInterface;

        //mfxRes = pSession->Init(implInterface, &par.Version);
        mfxRes = pSession->InitEx(init_param);

        // check the library version
        MFXQueryVersion(pSession, &libver);

        // check the numbers
        if ((libver.Major != par.Version.Major) ||
            (libver.Minor < par.Version.Minor))
        {
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_MEMORY_ALLOC;
    }
    if (MFX_ERR_NONE != mfxRes &&
        MFX_WRN_PARTIAL_ACCELERATION != mfxRes)
    {
        if (pSession)
        {
            delete pSession;
        }

        return mfxRes;
    }

    // save the handle
    *session = dynamic_cast<_mfxSession *>(pSession);

    return mfxRes;

} // mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session)

mfxStatus MFXDoWork(mfxSession session)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXDoWork");

    // check error(s)
    if (0 == session)
    {
        return MFX_ERR_INVALID_HANDLE;
    }

    MFXIUnknown * pInt = session->m_pScheduler;
    MFXIScheduler2 *newScheduler = 
        ::QueryInterface<MFXIScheduler2>(pInt, MFXIScheduler2_GUID);

    if (!newScheduler)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    newScheduler->Release();

    mfxStatus res = newScheduler->DoWork();    

    return res;
} // mfxStatus MFXDoWork(mfxSession *session)

mfxStatus MFXClose(mfxSession session)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXClose");
    mfxStatus mfxRes = MFX_ERR_NONE;

    // check error(s)
    if (0 == session)
    {
        return MFX_ERR_INVALID_HANDLE;
    }

    try
    {
        // parent session can't be closed,
        // because there is no way to let children know about parent's death.
        
        // child session should be uncoupled from the parent before closing.
        if (session->IsChildSession())
        {
            mfxRes = MFXDisjoinSession(session);
            if (MFX_ERR_NONE != mfxRes)
            {
                return mfxRes;
            }
        }
        
        if (session->IsParentSession())
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        
        // deallocate the object
#if defined(MFX_USE_VERSIONED_SESSION)
        _mfxSession_1_10 *newSession  = (_mfxSession_1_10 *)session;
        delete newSession;
#else
        delete session;
#endif
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
    }
#if defined(MFX_TRACE_ENABLE) && defined(MFX_VA)
    MFX_TRACE_CLOSE();
#endif
    return mfxRes;

} // mfxStatus MFXClose(mfxHDL session)

#if defined(_WIN32) || defined(_WIN64)
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    // touch unreferenced parameters
    g_hModule = hModule;
    lpReserved = lpReserved;

    // initialize the IPP library
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ippInit();
        break;

    default:
        break;
    }

    return TRUE;

} // BOOL APIENTRY DllMain(HMODULE hModule,
#else // #if defined(_WIN32) || defined(_WIN64)
void __attribute__ ((constructor)) dll_init(void)
{
    ippInit();
}
#endif // #if defined(_WIN32) || defined(_WIN64)
