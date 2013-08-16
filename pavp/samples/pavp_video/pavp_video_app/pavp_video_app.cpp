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

#include "pavp_video_lib_dx9.h"
#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
#include "pavp_video_lib_dx11.h"
#endif
#include "intel_pavp_gpucp_api.h"

int _tmain(int argc, TCHAR *argv[])
{
    HRESULT                hr = S_OK;
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    IDirect3D9Ex        *pd3d9 = NULL;
    IDirect3DDevice9Ex    *pd3d9Device = NULL;
    SAuthChannel9 *authChannel9 = NULL;
#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
    ID3D11Device *pD3D11Device = NULL;
    ID3D11DeviceContext *pD3D11Ctx = NULL;
    SAuthChannel11 *authChannel11 = NULL;
#endif
    CCPDevice *cpDevice = NULL;
    CPAVPSession *pavpSession = NULL;
    CPAVPVideo *pavpVideo = NULL;

    pavpSession = new CSIGMASession();

#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
    GUID guidTmp;
    bool useD3D11 = false;
    if (useD3D11)
    {
        static D3D_FEATURE_LEVEL FeatureLevels[] = { 
            D3D_FEATURE_LEVEL_11_1, 
            D3D_FEATURE_LEVEL_11_0, 
            D3D_FEATURE_LEVEL_10_1, 
            D3D_FEATURE_LEVEL_10_0 
        };
        D3D_FEATURE_LEVEL pFeatureLevelsOut;

        hr =  D3D11CreateDevice(
            NULL,    // provide real adapter
            D3D_DRIVER_TYPE_HARDWARE,
            NULL,
            0,
            NULL,//FeatureLevels,
            0,//PAVPSDK_COUNT_OF(FeatureLevels),
            D3D11_SDK_VERSION,
            &pD3D11Device,
            &pFeatureLevelsOut,
            &pD3D11Ctx);
        if( FAILED(hr) )
            goto end;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("ID3D11Device and ID3D11DeviceContext created succesfully.\n")));

        if (NULL == cpDevice)
        {
            GUID cryptoType = GUID_NULL; // counter and encryption type will be choosen depending on capabilities returned

            GUID profile = DXVA2_ModeH264_VLD_NoFGT;
            hr = CCPDeviceD3D11_create(
                pD3D11Device,
                profile, 
                PAVP_ENCRYPTION_AES128_CTR,
                D3D11_CONTENT_PROTECTION_CAPS_SOFTWARE | D3D11_CONTENT_PROTECTION_CAPS_HARDWARE,
                &cpDevice,
                &cryptoType);
            if (FAILED(hr))
            {
                PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceD3D11 (hr=0x%08x)\n"), hr));
                goto end;
            }
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("DX11 content protection CryptoSession interface created for:")));
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tprofile="))); dynamic_cast<CCPDeviceD3D11*>(cpDevice)->GetCryptoSessionPtr()->GetDecoderProfile(&guidTmp); PAVPSDK_printGUID(guidTmp);
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tcryptoType="))); dynamic_cast<CCPDeviceD3D11*>(cpDevice)->GetCryptoSessionPtr()->GetCryptoType(&guidTmp); PAVPSDK_printGUID(guidTmp);
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tKeyExchange="))); PAVPSDK_printGUID(dynamic_cast<CCPDeviceD3D11Base*>(cpDevice)->GetKeyExchangeGUID());
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n")));

            hr = SAuthChannel11_Create(&authChannel11, cpDevice);
            if (FAILED(hr))
            {
                PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create authenticated channel (hr=0x%08x)\n"), hr));
                goto end;
            } 
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("DX11 authenticated channel opened succesfully\n")));
        }
    }
    else
#endif //(NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8

    {
        hr = CreateD3D9AndDeviceEx( &pd3d9, &pd3d9Device );
        if( FAILED(hr) )
            goto end;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("IDirect3D9Ex and IDirect3DDevice9Ex CryptoSession created succesfully.\n")));

        if (NULL == cpDevice && PAVPCryptoSession9_isPossible())
        {
            HANDLE cs9Handle = 0;
            
            GUID profile = DXVA2_ModeH264_VLD_NoFGT;
            hr = CCPDeviceCryptoSession9_create(
                pd3d9Device,
                profile, 
                PAVP_ENCRYPTION_AES128_CTR,
                D3DCPCAPS_HARDWARE | D3DCPCAPS_SOFTWARE,
                &cpDevice,
                &cs9Handle,
                NULL);
            if (FAILED(hr))
            {
                PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceCryptoSession9 (hr=0x%08x)\n"), hr));
                goto end;
            }
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("DX9 content protection interface created for:")));
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tprofile="))); PAVPSDK_printGUID(dynamic_cast<CCPDeviceCryptoSession9*>(cpDevice)->GetProfile());
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tcryptoType="))); PAVPSDK_printGUID(dynamic_cast<CCPDeviceCryptoSession9*>(cpDevice)->GetCryptoType());
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tPAVPKeyExchange="))); PAVPSDK_printGUID(dynamic_cast<CCPDeviceCryptoSession9*>(cpDevice)->GetPAVPKeyExchangeGUID());
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n")));

            hr = SAuthChannel9_Create(&authChannel9, cpDevice, pd3d9Device);
            if (FAILED(hr))
            {
                PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create authenticated channel (hr=0x%08x)\n"), hr));
                goto end;
            } 
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("DX9 authenticated channel opened succesfully\n")));
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
                sts = PAVP_STATUS_NOT_SUPPORTED;
                PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceCryptoSession9 (hr=0x%08x)\n"), hr));
                goto end;
            }
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("DX9 content protection Intel Proprietary interface created for:")));
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tsessionType="))); PAVPSDK_printNumber(dynamic_cast<CCPDeviceAuxiliary9*>(cpDevice)->GetSessionType());
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tPAVPKeyExchange="))); PAVPSDK_printNumber(dynamic_cast<CCPDeviceAuxiliary9*>(cpDevice)->GetPAVPKeyExchangeFuncId());
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n")));
        }
    }

    if (NULL == cpDevice)
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("No interface exists to open PAVP session.\n")));
        sts = PAVP_STATUS_NOT_SUPPORTED;
        goto end;
    }
    
    SYSTEMTIME seed;
    GetSystemTime(&seed);
    sts = dynamic_cast<CSIGMASession*>(pavpSession)->Open(
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

    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("SIGMA session opened succesfully:")));
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tcerificate API version=0x%08x"), dynamic_cast<CSIGMASession*>(pavpSession)->GetCertAPIVersion()));
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tactual PAVP API version=0x%08x"), pavpSession->GetActualAPIVersion()));
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n\tsessionID=%d"), pavpSession->GetSessionId()));
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("\n")));

#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
    if (NULL != dynamic_cast<CCPDeviceD3D11*>(cpDevice))
    {
        CPAVPVideo_D3D11 * _v = new CPAVPVideo_D3D11(
            dynamic_cast<CCPDeviceD3D11*>(cpDevice),
            pavpSession);
        sts = _v->PreInit(NULL);
        pavpVideo = _v;
    }
    else
#endif //(NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
        if (NULL != dynamic_cast<CCPDeviceCryptoSession9*>(cpDevice))
    {
        CPAVPVideo_CryptoSession9 *_v = new CPAVPVideo_CryptoSession9(
            dynamic_cast<CCPDeviceCryptoSession9*>(cpDevice),
            pavpSession);
        sts = _v->PreInit(NULL);
        pavpVideo = _v;
    }
    else if (NULL != dynamic_cast<CCPDeviceAuxiliary9*>(cpDevice))
    {
        CPAVPVideo_Auxiliary9 *_v = new CPAVPVideo_Auxiliary9(
            dynamic_cast<CCPDeviceAuxiliary9*>(cpDevice),
            pavpSession);
        sts = _v->PreInit(NULL, NULL);
        pavpVideo = _v;
    }
    if (PAVP_EPID_FAILURE(sts))
    {
        PAVPSDK_SAFE_DELETE(pavpVideo);
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to PreInit Video processing pipeline protection (sts=0x%08x)\n"), sts));
        goto end;
    }

    if (NULL == pavpVideo)
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create Video processing pipeline protection\n")));
        goto end;
    }

    uint8 iv_ctr[16];
    memset (iv_ctr, 0, sizeof (iv_ctr));

    sts = pavpVideo->Init(0, iv_ctr, sizeof(iv_ctr)); 
    if (PAVP_EPID_FAILURE(sts))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to init Video processing pipeline protection (sts=0x%08x)\n"), sts));
        goto end;
    }

    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Video protection initialized succesfully.\n")));

    for (int i = 0; i < 10; i++)
    {
        uint8 data[16];
        
        sts = pavpVideo->Encrypt(data, data, 16, iv_ctr, sizeof(iv_ctr));
        if (0==i%2 || 
            PAVP_STATUS_REFRESH_REQUIRED_ERROR == sts)
        {
            pavpVideo->EncryptionKeyRefresh();
            //retry
            sts = pavpVideo->Encrypt(data, data, 16, iv_ctr, sizeof(iv_ctr));
            if (PAVP_EPID_FAILURE(sts))
            {
                PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to encrypt (sts=0x%08x)\n"), sts));
                goto end;
            }
        }
    }
    pavpVideo->Close();

    pavpSession->Close();

end:
    if (NULL != authChannel9)
    {
        SAuthChannel9_Destroy(authChannel9);
        authChannel9 = NULL;
    }
#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
    if (NULL != authChannel11)
    {
        SAuthChannel11_Destroy(authChannel11);
        authChannel11 = NULL;
    }
#endif

    PAVPSDK_SAFE_DELETE(pavpVideo);
    PAVPSDK_SAFE_DELETE(pavpSession);
    PAVPSDK_SAFE_DELETE(cpDevice);

    return sts;
}