/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

// mfx_pavp.cpp : Defines the entry point for the DLL application.
//
#include <initguid.h>

#include <windows.h>
#include "mfx_pavp.h"
#define PCPVD_EXPORTS
#define PCPVD_DLL
#include "pcp.h"

/*#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
*/
#pragma comment(lib, "ippcp_6_1.lib")
#pragma comment(lib, "pavp_dx11.lib")
#pragma comment(lib, "pavp_dx9.lib")
#pragma comment(lib, "pavp_dx9auxiliary.lib")
#pragma comment(lib, "pavp_sample_common.lib")
#pragma comment(lib, "pavp_video_lib.lib")
#pragma comment(lib, "sigma.lib")
#pragma comment(lib, "win8ui_video_lib.lib")
#pragma comment(lib, "epid_1_0.lib")
#pragma comment(lib, "sample_protected.lib")

#include <d3d9.h>
#include <d3d9types.h>
#include "pavp_video_lib_dx9.h"
#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
#include "pavp_video_lib_dx11.h"
#endif
#include "intel_pavp_gpucp_api.h"

#include "mfx_utils.h"

#ifdef _MANAGED
#pragma managed(push, off)
#endif

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

extern "C" CPAVPSession * CreateCSIGMASession(
        CCPDevice *cpDevice,
        uint32 certAPIVersion,
        const uint8 *cert, 
        uint32 certSize, 
        const void **revocationLists, 
        uint32 revocationListsCount,
        const EcDsaPrivKey privKey,
        const uint32 *seed,
        uint32 seedBits)
{
    CSIGMASession *session = new CSIGMASession();
    PavpEpidStatus PavpStatus = session->Open(
        cpDevice, 
        certAPIVersion, 
        cert, certSize, 
        NULL, 0,
        privKey,
        (uint32*)&seed, sizeof(seed));
    if (PAVP_EPID_FAILURE(PavpStatus))
    {
        delete session;
        return NULL;
    }
    return session;
}

extern "C" CVideoProtection* CreateCVideoProtection_CS11(
    CCPDeviceD3D11Base* cpDevice)
{
    return new CVideoProtection_CS11(cpDevice);
}


extern "C" CPAVPVideo* CreateCPAVPVideo_D3D11(
    CCPDeviceD3D11* cpDevice, 
    CPAVPSession *session, 
    const PAVP_ENCRYPTION_MODE *decryptorMode)
{
    CPAVPVideo_D3D11 *v = new CPAVPVideo_D3D11(cpDevice, session);
    if (NULL == v)
        return NULL;
    PavpEpidStatus PavpStatus = v->PreInit(decryptorMode);
    if (PAVP_EPID_FAILURE(PavpStatus))
    {
        delete v;
        return NULL;
    }
    return v;
}

extern "C" CPAVPVideo* CreateCPAVPVideo_CryptoSession9(
    CCPDeviceCryptoSession9* cpDevice, 
    CPAVPSession *session, 
    const PAVP_ENCRYPTION_MODE *decryptorMode)
{
    CPAVPVideo_CryptoSession9 *v = new CPAVPVideo_CryptoSession9(cpDevice, session);
    if (NULL == v)
        return NULL;
    PavpEpidStatus PavpStatus = v->PreInit(decryptorMode);
    if (PAVP_EPID_FAILURE(PavpStatus))
    {
        delete v;
        return NULL;
    }
    return v;
}


extern "C" CPAVPVideo* CreateCPAVPVideo_Auxiliary9(
    CCPDeviceAuxiliary9* cpDevice, 
    CPAVPSession *session, 
    const PAVP_ENCRYPTION_MODE *encryptorMode,
    const PAVP_ENCRYPTION_MODE *decryptorMode)
{
    CPAVPVideo_Auxiliary9 *v = new CPAVPVideo_Auxiliary9(cpDevice, session);
    if (NULL == v)
        return NULL;
    PavpEpidStatus PavpStatus = v->PreInit(encryptorMode, decryptorMode);
    if (PAVP_EPID_FAILURE(PavpStatus))
    {
        delete v;
        return NULL;
    }
    return v;
}


extern "C" void mfxPAVPDestroy(void * ptr)
{
    delete ptr;
    ptr = nullptr;
}   
