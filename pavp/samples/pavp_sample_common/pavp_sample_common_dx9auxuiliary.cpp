/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include <tchar.h>

#include <d3d9.h>
#include <dxva2api.h>

#include "pavp_sample_common_dx9.h"
#include "intel_pavp_gpucp_api.h"


HRESULT CCPDeviceAuxiliary9_create(
    IDirect3DDevice9Ex* pd3d9Device,
    PAVP_SESSION_TYPE sessionType,
    UINT AvailableMemoryProtectionMask, 
    CCPDevice** hwDevice)
{
    HRESULT                hr = S_OK;

    IDirectXVideoDecoder* pAuxDevice = NULL;
    PAVP_QUERY_CAPS caps;

    hr = PAVPAuxiliary9_create(pd3d9Device, &pAuxDevice);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create auxiliary device (hr=0x%08x)\n"), hr));
        goto end;
    } 

    hr = PAVPAuxiliary9_getPavpCaps(pAuxDevice, &caps);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to query auxiliary device for PAVP capabilities (hr=0x%08x)\n"), hr));
        goto end;
    } 

    if (PAVP_MEMORY_PROTECTION_LITE & caps.AvailableMemoryProtection & AvailableMemoryProtectionMask)
        hr = PAVPAuxiliary9_initialize(pAuxDevice, PAVP_KEY_EXCHANGE_DAA, sessionType);
    else if (PAVP_MEMORY_PROTECTION_DYNAMIC & caps.AvailableMemoryProtection & AvailableMemoryProtectionMask)
        hr = PAVPAuxiliary9_initialize(pAuxDevice, PAVP_KEY_EXCHANGE_HEAVY, sessionType);
    else
    {
        hr = E_FAIL;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to initialize auxiliary device: unknown memory protection\n")));
        goto end;
    }
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to initialize auxiliary device (hr=0x%08x)\n"), hr));
        goto end;
    } 

    *hwDevice = new CCPDeviceAuxiliary9(pAuxDevice, PAVP_KEY_EXCHANGE, sessionType);
    if (NULL == *hwDevice)
    {
        hr = E_OUTOFMEMORY;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceAuxiliary9\n")));
        goto end;
    }
end:
    if (FAILED(hr))
        PAVPSDK_SAFE_DELETE(*hwDevice);
    PAVPSDK_SAFE_RELEASE(pAuxDevice);
    
    return hr;
}
