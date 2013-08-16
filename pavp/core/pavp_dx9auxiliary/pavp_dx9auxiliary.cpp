/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include <windows.h>
#include <intel_auxiliary_api.h>
#include <dwmapi.h>

#include "pavp_dx9auxiliary.h"

#define MIN_SURF_WIDTH    16
#define MIN_SURF_HEIGHT    16

bool PAVPAuxiliary9_isPossible()
{
    OSVERSIONINFO  versionInfo;
    BOOL           bRet;

    ZeroMemory(&versionInfo, sizeof(OSVERSIONINFO));
    versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    bRet = GetVersionEx(&versionInfo);
    if (bRet == FALSE)
        return false;

    BOOL isAero = FALSE;
    HRESULT hr = DwmIsCompositionEnabled(&isAero);
    if (FAILED(hr))
        isAero = FALSE;

    // PAVP via auxiliary interface supported on Vista for both Aero ON and OFF.
    // For Windows 7 if is supported for only Aero OFF.
    return (versionInfo.dwMajorVersion >= 6) &&
        (versionInfo.dwMinorVersion == 0 || (versionInfo.dwMinorVersion >= 1 && !isAero)) &&
        (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT);
}

HRESULT PAVPAuxiliary9_create(IDirect3DDevice9 *pD3DDevice9, IDirectXVideoDecoder** ppAuxDevice)
{
    HRESULT        hr = E_FAIL;
    UINT        decoderGuidCount = 0, targetCount = 0, cfgCount = 0;
    UINT        index = 0;
    
    GUID        *pDecoderGuids = NULL;
    D3DFORMAT    *pFmts = NULL;

    DXVA2_VideoDesc                vidDesc = {0};
    DXVA2_ConfigPictureDecode    *pCfg = NULL;
    
    IDirect3DSurface9*           pDummySurf = NULL;
    IDirectXVideoDecoderService* pDecoderService = NULL;
    IDirectXVideoDecoder*        pAuxDevice = NULL;

    if( NULL == pD3DDevice9 || NULL == ppAuxDevice)
        return E_POINTER;

    do
    {

        // Get the DXVA2 Decoder Service interface
        // (The Intel Auxiliary device interface is implemented through the decoder interfaces)
        hr = DXVA2CreateVideoService(pD3DDevice9, IID_IDirectXVideoDecoderService, (void **)&pDecoderService);
        if( FAILED(hr) )
        {
            break;
        }
            
        // Get the list of devices supported
        hr = pDecoderService->GetDecoderDeviceGuids(&decoderGuidCount, &pDecoderGuids);
        if( FAILED(hr) )
        {
            break;
        }

        // Look for the Intel Auxiliary device GUID
        for(index = 0; index < decoderGuidCount; index++)
            if(DXVA2_Intel_Auxiliary_Device == pDecoderGuids[index])
                break;
        if( decoderGuidCount == index )
        {
            hr = E_FAIL;
            break;
        }

        // Use the GetDecodeRenderTargets to get the format needed for the dummy surface
        // that will be passed in the call to create the device. This should only return 1 format.
        hr = pDecoderService->GetDecoderRenderTargets(pDecoderGuids[index], &targetCount, &pFmts);
        if( FAILED(hr) || 0 >= targetCount )
        {
            hr = E_FAIL;
            break;
        }

        // Get the decoder configuration. 
        // Not really needed, but necessary to create a DXVA2 decode device.
        hr = pDecoderService->GetDecoderConfigurations(pDecoderGuids[index], &vidDesc, NULL, &cfgCount, &pCfg);
        if( FAILED(hr) || 0 >= cfgCount )
        {
            hr = E_FAIL;
            break;
        }

        // Create a dummy surface. This surface has no use, but is required by the DXVA2
        // call to create a decoder device. Create the minimum size possible: 16x16
        hr = pDecoderService->CreateSurface(    MIN_SURF_WIDTH, MIN_SURF_HEIGHT, 0, (D3DFORMAT)'3CMI' /*pFmts[0]*/, 
                                                D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, 
                                                &pDummySurf, NULL);
        if( FAILED(hr) )
        {
            break;
        }

        // Create the DXVA2 Decoder device that represents the Auxiliary device service interface
        hr = pDecoderService->CreateVideoDecoder( pDecoderGuids[index], &vidDesc, pCfg, &pDummySurf, 1, &pAuxDevice);
        if( FAILED(hr) )
        {
            break;
        }

        hr = S_OK;

    } while( FALSE );

    if( pDecoderGuids )
    {
        CoTaskMemFree(pDecoderGuids);
        pDecoderGuids = NULL;
    }

    if( pFmts )
    {
        CoTaskMemFree(pFmts);
        pFmts = NULL;
    }
    
    if( pCfg )
    {
        CoTaskMemFree(pCfg);
        pCfg = NULL;
    }

    if( pDummySurf )
    {
        pDummySurf->Release();
        pDummySurf = NULL;
    }

    if( pDecoderService )
    {
        pDecoderService->Release();
        pDecoderService = NULL;
    }

    if (!FAILED(hr))
        *ppAuxDevice = pAuxDevice;

    return hr;
}

HRESULT PAVPAuxiliary9_getPavpCaps(IDirectXVideoDecoder* aux, PAVP_QUERY_CAPS *pCaps)
{
    PavpEpidStatus      PavpStatus = PAVP_STATUS_SUCCESS;
    HRESULT             hr = S_OK;

    if (NULL == aux || NULL == pCaps)
        return E_POINTER;

    DXVA2_DecodeExecuteParams    execParams = {0};
    DXVA2_DecodeExtensionData    execExtData = {0};

    execParams.pExtensionData = &execExtData;
    execExtData.Function = AUXDEV_QUERY_ACCEL_CAPS;
    execExtData.pPrivateInputData = (void*)&DXVA2_Intel_Pavp;
    execExtData.PrivateInputDataSize = sizeof(GUID);
    execExtData.pPrivateOutputData = pCaps;
    execExtData.PrivateOutputDataSize = sizeof(PAVP_QUERY_CAPS);

    return aux->Execute(&execParams);
    
}

HRESULT PAVPAuxiliary9_getPavpCaps2(IDirectXVideoDecoder* aux, PAVP_QUERY_CAPS2 *pCaps)
{
    PavpEpidStatus      PavpStatus = PAVP_STATUS_SUCCESS;
    HRESULT             hr = S_OK;

    if (NULL == aux || NULL == pCaps)
        return E_POINTER;

    DXVA2_DecodeExecuteParams    execParams = {0};
    DXVA2_DecodeExtensionData    execExtData = {0};

    execParams.pExtensionData = &execExtData;
    execExtData.Function = AUXDEV_QUERY_ACCEL_CAPS;
    execExtData.pPrivateInputData = (void*)&DXVA2_Intel_Pavp;
    execExtData.PrivateInputDataSize = sizeof(GUID);
    execExtData.pPrivateOutputData = pCaps;
    execExtData.PrivateOutputDataSize = sizeof(PAVP_QUERY_CAPS2);

    return aux->Execute(&execParams);
    
}

HRESULT PAVPAuxiliary9_initialize(
    IDirectXVideoDecoder* aux, 
    PAVP_KEY_EXCHANGE_MASK keyExchange,
    PAVP_SESSION_TYPE sessionType)
{
    HRESULT hr = S_OK;
    DXVA2_DecodeExecuteParams    execParams = {0};
    DXVA2_DecodeExtensionData    execExtData = {0};
    execParams.pExtensionData = &execExtData;

/* TODO: verify later. Driver only reports DAA in AvailableKeyExchangeProtocols but should also report HEAVY
    // Check requested key exchange is available
    PAVP_QUERY_CAPS sQueryCaps;
    execParams.pExtensionData->Function                = AUXDEV_QUERY_ACCEL_CAPS;
    execParams.pExtensionData->pPrivateInputData        = (void*)&DXVA2_Intel_Pavp;
    execParams.pExtensionData->PrivateInputDataSize    = sizeof(GUID);
    execParams.pExtensionData->pPrivateOutputData        = &sQueryCaps;
    execParams.pExtensionData->PrivateOutputDataSize    = sizeof(sQueryCaps);
    hr = aux->Execute(&execParams);
    if( FAILED(hr) )
        return hr;//PAVP_STATUS_NOT_SUPPORTED;
    if( !(sQueryCaps.AvailableKeyExchangeProtocols & keyExchange) )
        return E_FAIL;//PAVP_STATUS_KEY_EXCHANGE_TYPE_NOT_SUPPORTED;
*/
    PAVP_CREATE_DEVICE sCreateStruct1;
    PAVP_CREATE_DEVICE2 sCreateStruct2;
    execParams.pExtensionData->Function                = AUXDEV_CREATE_ACCEL_SERVICE;
    execParams.pExtensionData->pPrivateInputData        = (void*)&DXVA2_Intel_Pavp;
    execParams.pExtensionData->PrivateInputDataSize    = sizeof(GUID);
    if (PAVP_SESSION_TYPE_DECODE == sessionType)
    {
        sCreateStruct1.eDesiredKeyExchangeMode = keyExchange;

        execParams.pExtensionData->pPrivateOutputData        = &sCreateStruct1;
        execParams.pExtensionData->PrivateOutputDataSize    = sizeof(sCreateStruct1);
    }
    else
    {
        sCreateStruct2.eDesiredKeyExchangeMode = keyExchange;
        sCreateStruct2.ePavpSessionType        = sessionType;
        sCreateStruct2.PavpVersion = 1;

        execParams.pExtensionData->pPrivateOutputData        = &sCreateStruct2;
        execParams.pExtensionData->PrivateOutputDataSize    = sizeof(sCreateStruct2);
    }
    hr = aux->Execute(&execParams);
    if( FAILED(hr) )
        return hr;
    
    return hr;
}

CCPDeviceAuxiliary9::CCPDeviceAuxiliary9(
    IDirectXVideoDecoder* aux, 
    uint32 PAVPKeyExchangeFuncId, 
    PAVP_SESSION_TYPE sessionType)
        : m_aux(aux), 
        m_PAVPKeyExchangeFuncId(PAVPKeyExchangeFuncId), 
        m_sessionType(sessionType)
{
    if (NULL != m_aux)
        m_aux->AddRef();
}

CCPDeviceAuxiliary9::~CCPDeviceAuxiliary9()
{
    PAVPSDK_SAFE_RELEASE(m_aux);
}

PavpEpidStatus CCPDeviceAuxiliary9::ExecuteKeyExchange(
    const void *pDataIn,
    uint32 uiSizeIn,
    void *pOutputData,
    uint32 uiSizeOut)
{
    return ExecuteFunc(m_PAVPKeyExchangeFuncId, pDataIn, uiSizeIn, pOutputData, uiSizeOut);
}

PavpEpidStatus CCPDeviceAuxiliary9::ExecuteFunc(
    uint32 uiFuncID,
    const void *pDataIn,
    uint32 uiSizeIn,
    void *pOutputData,
    uint32 uiSizeOut)
{
    HRESULT hr = E_FAIL;

    if (NULL == m_aux)
        return E_POINTER;

    DXVA2_DecodeExecuteParams    execParams = {0};
    DXVA2_DecodeExtensionData    execExtData = {0};

    execParams.pExtensionData = &execExtData;
    execExtData.Function = uiFuncID;
    execExtData.pPrivateInputData = (void*)pDataIn;
    execExtData.PrivateInputDataSize = uiSizeIn;
    execExtData.pPrivateOutputData = pOutputData;
    execExtData.PrivateOutputDataSize = uiSizeOut;

    hr = m_aux->Execute(&execParams);
    if( FAILED(hr) )
        return PAVP_STATUS_UNKNOWN_ERROR;

    return PAVP_STATUS_SUCCESS;
}