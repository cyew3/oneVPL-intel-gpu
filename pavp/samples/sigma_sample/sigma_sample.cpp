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
#include <stdio.h>

#include <windows.h>
#include <initguid.h>

#include "pavp_sample_common_dx9.h"
#include "pavpsdk_sigma.h"

#include <vector>
using namespace std;

#ifdef _WIN64
#define GET_RDTSC(RDTSC) { \
    *(RDTSC) = __rdtsc();\
    }
#else //_WIN64
#define GET_RDTSC(RDTSC) { \
    __int64 * Dst = RDTSC;         \
    _asm { pushad            }     \
    _asm { mov ebx, Dst      }     \
    _asm { _emit 0x0f        }     \
    _asm { _emit 0x31        }     \
    _asm { mov [ebx], eax    }     \
    _asm { mov [ebx+4], edx  }     \
    _asm { popad             } }
#endif //_WIN64

HRESULT GetDX9DecoderGuids(IDirect3DDevice9Ex *pd3d9Device, GUID encryptionRequired, vector<GUID> &decodeProfiles)
{
    HRESULT                hr = S_OK;
    IDirectXVideoDecoderService* pAccelServices = NULL;
    DXVA2_ConfigPictureDecode *pConfigs=NULL;
    GUID* guids = NULL;
    UINT  nguids;
    int i, j;

    hr = DXVA2CreateVideoService(pd3d9Device, IID_IDirectXVideoDecoderService,(void**)&pAccelServices);
    if( FAILED(hr) )
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("DXVA2CreateVideoService fails (hr=0x%08x).\n"), hr));
        goto end;
    }

    hr=pAccelServices->GetDecoderDeviceGuids(&nguids, &guids);
    if( FAILED(hr) )
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("IDirectXVideoDecoderService::GetDecoderDeviceGuids fails (hr=0x%08x).\n"), hr));
        goto end;
    }

    for (i=0;(UINT)i<nguids;i++)
    {
        if (GUID_NULL != guids[i])
        {
            UINT nConfigs=0;
            pConfigs=NULL;
            DXVA2_VideoDesc desc;
            hr=pAccelServices->GetDecoderConfigurations(guids[i],&desc,0,&nConfigs,&pConfigs);
            if( FAILED(hr) )
            {
                PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("IDirectXVideoDecoderService::GetDecoderConfigurations fails (hr=0x%08x).\n"), hr));
                goto end;
            }
            for (j = 0; (UINT)j < nConfigs; j++)
                if (encryptionRequired == pConfigs[j].guidConfigBitstreamEncryption)
                    break;
            CoTaskMemFree(pConfigs);
            pConfigs=NULL;
            if (j != nConfigs)
                decodeProfiles.push_back(guids[i]);
        }
        
    }

end:
    CoTaskMemFree(guids);
    PAVPSDK_SAFE_RELEASE(pAccelServices);

    return hr;
}


bool VeritySIGMASupportAndPreCompute_DX9(
        uint32 certAPIVersion,
        const uint8 *cert, 
        uint32 certSize, 
        const void **revocationLists, 
        uint32 revocationListsCount,
        const EcDsaPrivKey privKey,
        uint8 blob[4672/8], EpidCert *pchCert)
{
    bool result = false;

    HRESULT                hr = S_OK;
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    IDirect3D9Ex        *pd3d9 = NULL;
    IDirect3DDevice9Ex    *pd3d9Device = NULL;
    CCPDevice *cpDevice = NULL;
    CSIGMASession sigma;
    
    hr = CreateD3D9AndDeviceEx( &pd3d9, &pd3d9Device );
    if( FAILED(hr) )
        goto end;

    if (NULL == cpDevice && PAVPCryptoSession9_isPossible())
    {
        vector<GUID> decodeProfiles;
        hr = GetDX9DecoderGuids(pd3d9Device, DXVA2_Intel_Pavp, decodeProfiles);
        if( FAILED(hr) )
            goto end;
        if (0 == decodeProfiles.size())
        {
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Error: No decoder profile found supports DXVA2_Intel_Pavp bitstream encryption.\n")));
            goto end;
        }

        HANDLE cs9Handle = 0;
        GUID cryptoType = GUID_NULL;

        hr = CCPDeviceCryptoSession9_create(
            pd3d9Device,
            decodeProfiles[0], 
            PAVP_ENCRYPTION_AES128_CTR,
            D3DCPCAPS_HARDWARE | D3DCPCAPS_SOFTWARE,
            &cpDevice,
            &cs9Handle,
            &cryptoType);
        if (FAILED(hr))
        {
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("CCPDeviceCryptoSession9_create fails (hr=0x%08x).\n"), hr));
            goto end;
        }
    }
    
    if (NULL == cpDevice && PAVPAuxiliary9_isPossible())
    {
        hr = CCPDeviceAuxiliary9_create(
            pd3d9Device,
            PAVP_SESSION_TYPE_DECODE,
            PAVP_MEMORY_PROTECTION_DYNAMIC | PAVP_MEMORY_PROTECTION_LITE,
            &cpDevice);
        if (FAILED(hr))
        {
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("CCPDeviceAuxiliary9_create fails (hr=0x%08x).\n"), hr));
            goto end;
        }
    }

    if (NULL == cpDevice)
        goto end;


    SYSTEMTIME seed;
    GetSystemTime(&seed);
    sts = sigma.Open(
        cpDevice, 
        certAPIVersion, 
        cert, certSize, 
        revocationLists, revocationListsCount, 
        privKey,
        (uint32*)&seed, sizeof(seed));
    if (PAVP_EPID_FAILURE(sts))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to open SIGMA session (sts=0x%08x).\n"), sts));
        return false;
    }

    uint32 blobSize = 4672/8;
    sts = sigma.GetPreComputationBlob(blob, &blobSize, pchCert);
    sigma.Close();
    if (PAVP_EPID_FAILURE(sts))
        goto end;

    result = true;
end:
    PAVPSDK_SAFE_DELETE(cpDevice);
    PAVPSDK_SAFE_RELEASE(pd3d9Device);
    PAVPSDK_SAFE_RELEASE(pd3d9);

    return result;
}


int _tmain(int argc, TCHAR *argv[])
{
    __int64 sTime, eTime, sTime1, eTime1, freq;
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);

    GET_RDTSC(&sTime1);
    uint8 blob[4672/8];
    EpidCert pchCert;
    if (!VeritySIGMASupportAndPreCompute_DX9(
        Test_v10_ApiVersion, 
        Test_v10_Cert3p, Test_v10_Cert3pSize, 
        NULL, 0, 
        Test_v10_DsaPrivateKey, 
        blob, &pchCert))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("DX9 content protection is not supported\n")));
        return 0;
    }
    GET_RDTSC(&eTime1);
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("SIGMA session precomputed in %f msec\n"), ((double)(eTime1-sTime1))/freq));

    GET_RDTSC(&sTime);

    HRESULT                hr = S_OK;
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    IDirect3D9Ex        *pd3d9 = NULL;
    IDirect3DDevice9Ex    *pd3d9Device = NULL;
    CCPDevice *cpDevice = NULL;
    CSIGMASession sigma;    


    
    hr = CreateD3D9AndDeviceEx( &pd3d9, &pd3d9Device );
    if( FAILED(hr) )
        goto end;


    if (NULL == cpDevice && PAVPCryptoSession9_isPossible())
    {
        HANDLE cs9Handle = 0;
        GUID cryptoType = GUID_NULL;

        hr = CCPDeviceCryptoSession9_create(
            pd3d9Device,
            DXVA2_ModeH264_VLD_NoFGT, 
            PAVP_ENCRYPTION_AES128_CTR,
            D3DCPCAPS_HARDWARE | D3DCPCAPS_SOFTWARE,
            &cpDevice,
            &cs9Handle,
            &cryptoType);
        if (FAILED(hr))
        {
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceCryptoSession9 (hr=0x%08x)\n"), hr));
            goto end;
        }
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("DX9 content protection interface created for:")));
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tprofile=")); PAVPSDK_printGUID(dynamic_cast<CCPDeviceCryptoSession9*>(cpDevice)->GetProfile()));
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tcryptoType=")); PAVPSDK_printGUID(dynamic_cast<CCPDeviceCryptoSession9*>(cpDevice)->GetCryptoType()));
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tPAVPKeyExchange=")); PAVPSDK_printGUID(dynamic_cast<CCPDeviceCryptoSession9*>(cpDevice)->GetPAVPKeyExchangeGUID()));
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n")));
    }
    
    if (NULL == cpDevice && PAVPAuxiliary9_isPossible())
    {
        hr = CCPDeviceAuxiliary9_create(
            pd3d9Device,
            PAVP_SESSION_TYPE_DECODE,
            PAVP_MEMORY_PROTECTION_DYNAMIC | PAVP_MEMORY_PROTECTION_LITE,
            &cpDevice);
        if (FAILED(hr))
        {
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceCryptoSession9 (hr=0x%08x)\n"), hr));
            goto end;
        }
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("DX9 content protection Intel Proprietary interface created for:")));
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tsessionType=")); PAVPSDK_printNumber(dynamic_cast<CCPDeviceAuxiliary9*>(cpDevice)->GetSessionType()));
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tPAVPKeyExchange=")); PAVPSDK_printNumber(dynamic_cast<CCPDeviceAuxiliary9*>(cpDevice)->GetPAVPKeyExchangeFuncId()));
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n")));
    }

    if (NULL == cpDevice)
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("No interface exists to open PAVP session.\n")));
        sts = PAVP_STATUS_UNKNOWN_ERROR;
        goto end;
    }

    GET_RDTSC(&sTime1);
    sts = sigma.SetPreComputationBlob(blob, &pchCert);
    if (PAVP_EPID_FAILURE(sts))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to set SIGMA pre computation (sts=0x%08x)\n"), sts));
        goto end;
    }

    SYSTEMTIME seed;
    GetSystemTime(&seed);

    sts = sigma.Open(
        cpDevice, 
        Test_v10_ApiVersion, 
        Test_v10_Cert3p, Test_v10_Cert3pSize, 
        NULL, 0, 
        Test_v10_DsaPrivateKey,
        (uint32*)&seed, sizeof(seed));
    if (PAVP_EPID_FAILURE(sts))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to open SIGMA session (sts=0x%08x)\n"), sts));
        goto end;
    }
    GET_RDTSC(&eTime1);


    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("SIGMA session opened succesfully in %f msec:"), ((double)(eTime1-sTime1))/freq));
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tcerificate API version=0x%08x"), sigma.GetCertAPIVersion()));
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tactual PAVP API version=0x%08x"), sigma.GetActualAPIVersion()));
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tsessionID=%d"), sigma.GetSessionId()));
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n")));

    sigma.Close();

end:
    PAVPSDK_SAFE_DELETE(cpDevice);
    PAVPSDK_SAFE_RELEASE(pd3d9Device);
    PAVPSDK_SAFE_RELEASE(pd3d9);

    GET_RDTSC(&eTime);
    _tprintf(_T("Run time excluding VeritySIGMASupportAndPreCompute_DX9 is %f msec\n"), ((double)(eTime-sTime))/freq);    

    return sts;
}