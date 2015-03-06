//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2011-2011 Intel Corporation. All Rights Reserved.
//

#include "private_query_device.h"

CPrivateQueryDevice::CPrivateQueryDevice(
    IDirect3DDevice9 *pD3DDevice9) :
    CIntelAuxiliaryDevice(pD3DDevice9),
    bIsPresent(FALSE)
{
    HRESULT hRes   = S_OK;
    UINT    i, uGuids;
    GUID   *pGUIDs = NULL;

    // Query Acceleration GUIDs
    hRes = QueryAccelGuids(&pGUIDs, &uGuids);
    if (FAILED(hRes))
    {
        DBGMSG((TEXT("Failed to obtain acceleration devices GUIDs (code 0x%x).\n"), hRes));
        goto cleanup;
    }

    // Check if Private Query GUID is present in the system
    for (i = 0; i < uGuids; i++)
    {
        if (IsEqualGUID(pGUIDs[i], DXVA2_PrivateQueryDevice))
        {
            bIsPresent = TRUE;
            break;
        }
    }

cleanup:
    if (pGUIDs) free(pGUIDs);
}

HRESULT CPrivateQueryDevice::QueryDualDxvaDecode(
    UINT            Width1,
    UINT            Height1,
    UINT            Width2,
    UINT            Height2,
    BOOL *          pSupported)
{
    QUERY_DUAL_DXVA_DECODE_V1 Query;
    UINT    uSize;
    HRESULT hRes = E_FAIL;

    if (!bIsPresent)
    {
        goto finish;
    }

    Query.Header.iPrivateQueryID = QUERY_ID_DUAL_DXVA_DECODE_V1;
    Query.Header.iReserved       = 0;
    Query.Width1  = Width1;
    Query.Height1 = Height1;
    Query.Width2  = Width2;
    Query.Height2 = Height2;

    uSize = sizeof(Query);

    hRes = QueryAccelCaps(&DXVA2_PrivateQueryDevice, &Query, &uSize);

    if (SUCCEEDED(hRes))
    {
        *pSupported = Query.bSupported;
    }

finish:
    return hRes;
}


HRESULT CPrivateQueryDevice::QueryDualDxvaDecodeV2(
    QUERY_STREAM_V2 Stream1,
    QUERY_STREAM_V2 Stream2,
    BOOL *          pSupported)
{
    QUERY_DUAL_DXVA_DECODE_V2 Query;
    UINT    uSize;
    HRESULT hRes = E_FAIL;

    if (!bIsPresent)
    {
        goto finish;
    }

    uSize = sizeof(Query);
    ZeroMemory(&Query, uSize);

    Query.Header.iPrivateQueryID = QUERY_ID_DUAL_DXVA_DECODE_V2;
    Query.Stream1 = Stream1;
    Query.Stream2 = Stream2;

    hRes = QueryAccelCaps(&DXVA2_PrivateQueryDevice, &Query, &uSize);

    if (SUCCEEDED(hRes))
    {
        *pSupported = Query.bSupported;
    }

finish:
    return hRes;
}


HRESULT CPrivateQueryDevice::SelectVppS3dMode(
    QUERY_S3D_MODE  S3D_mode)
{
    QUERY_SET_VPP_S3D_MODE Query;
    UINT    uSize;
    HRESULT hRes = E_FAIL;

    if (!bIsPresent)
    {
        goto finish;
    }

    Query.Header.iPrivateQueryID = QUERY_ID_SET_VPP_S3D_MODE;
    Query.Header.iReserved       = 0;
    Query.S3D_mode               = S3D_mode;

    uSize = sizeof(Query);

    hRes = QueryAccelCaps(&DXVA2_PrivateQueryDevice, &Query, &uSize);

finish:
    return hRes;
}