/* ****************************************************************************** *\

Copyright (C) 2018 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_dxloader.cpp

\* ****************************************************************************** */
#include "mfx_dxloader.h"

#if !defined(OPEN_SOURCE) && defined(MEDIASDK_DX_LOADER)

#include <d3d11.h>
#include <dxgi.h>

#define INTEL_VENDOR_ID 0x8086

namespace MFX
{
    /* Not correct singleton, need a map or smthng like this */
    MfxDxLoader* MfxDxLoader::GetMfxDxLoader()
    {
        static MfxDxLoader loader;
        return &loader;
    }

    mfxStatus MfxDxLoader::InitDevice(void)
    {
        HRESULT hr = S_OK;
#ifndef MEDIASDK_UWP_LOADER
        typedef HRESULT(WINAPI *FuncD3D11CreateDevice)(
            _In_opt_ IDXGIAdapter* pAdapter,
            D3D_DRIVER_TYPE DriverType,
            HMODULE Software,
            UINT Flags,
            _In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
            UINT FeatureLevels,
            UINT SDKVersion,
            _COM_Outptr_opt_ ID3D11Device** ppDevice,
            _Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
            _COM_Outptr_opt_ ID3D11DeviceContext** ppImmediateContext);
        HMODULE d3d11Mod = NULL;
        FuncD3D11CreateDevice CreateDevice = NULL;
#endif
        //        if (pAdapter == NULL)
        //            return MFX_ERR_NULL_PTR;

#ifndef MEDIASDK_UWP_LOADER
        d3d11Mod = GetModuleHandle(L"d3d11.dll");

        if (!d3d11Mod)
            return MFX_ERR_UNSUPPORTED;

        CreateDevice = (FuncD3D11CreateDevice)GetProcAddress(d3d11Mod, "D3D11CreateDevice");

        if (!CreateDevice)
            return MFX_ERR_UNSUPPORTED;

        hr = CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &pDevice, NULL, &pContext);
#else
        hr = D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &pDevice, NULL, &pContext);
#endif
        if (FAILED(hr))
        {
            ReleaseDevice();
            return MFX_ERR_DEVICE_FAILED;
        }

        hr = pDevice->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&pVDevice);
        if (FAILED(hr))
        {
            ReleaseDevice();
            return MFX_ERR_DEVICE_FAILED;
        }

        hr = pContext->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&pVContext);
        if (FAILED(hr))
        {
            ReleaseDevice();
            return MFX_ERR_DEVICE_FAILED;
        }

        return MFX_ERR_NONE;
    }

    mfxStatus MfxDxLoader::InitDecoder(void)
    {
        if (!pVDevice || !pVContext)
            return MFX_ERR_NULL_PTR;

        HRESULT hr = S_OK;

        mfxU32 uiCount = pVDevice->GetVideoDecoderProfileCount();
        if (uiCount <= 0)
        {
            ReleaseDevice();
            return MFX_ERR_UNSUPPORTED;
        }

        for (mfxU32 n = 0; n < uiCount; n++)
        {
            // Get Video Recorder Profile GUID
            hr = pVDevice->GetVideoDecoderProfile(n, &decGUID);
            if (SUCCEEDED(hr) && isMediaSDKGUID())
            {
                break;
            }
        }

        if (!isMediaSDKGUID())
        {
            ReleaseDevice();
            return MFX_ERR_UNSUPPORTED;
        }

        D3D11_VIDEO_DECODER_DESC decDesc = {};
        D3D11_VIDEO_DECODER_CONFIG decConfig = {};
        uint32_t configCount = 0;
        decDesc.Guid = MediaSDK_GUID;
        decDesc.SampleWidth = 1024;
        decDesc.SampleHeight = 768;
        decDesc.OutputFormat = DXGI_FORMAT_NV12;

        hr = pVDevice->GetVideoDecoderConfigCount(&decDesc, &configCount);
        if (FAILED(hr))
        {
            ReleaseDevice();
            return MFX_ERR_DEVICE_FAILED;
        }

        hr = pVDevice->GetVideoDecoderConfig(&decDesc, 0, &decConfig);
        if (FAILED(hr))
        {
            ReleaseDevice();
            return MFX_ERR_DEVICE_FAILED;
        }

        hr = pVDevice->CreateVideoDecoder(&decDesc, &decConfig, &pDecoder);
        if (FAILED(hr))
        {
            ReleaseDevice();
            return MFX_ERR_DEVICE_FAILED;
        }

        return MFX_ERR_NONE;
    }

    mfxStatus MfxDxLoader::GetEntryPoint(void** entrypoint, const LPCSTR entryName)
    {
        if (!pDecoder || !pVContext)
            return MFX_ERR_NULL_PTR;

        HRESULT hr = S_OK;

        D3D11_VIDEO_DECODER_EXTENSION ExtensionData = {};

        /* Query amount of entrypoints */
        ExtensionData.Function = 0x04030201;
        ExtensionData.pPrivateInputData = (void*)entryName;
        ExtensionData.PrivateInputDataSize = static_cast<UINT>(strlen(entryName));
        ExtensionData.pPrivateOutputData = entrypoint;
        ExtensionData.PrivateOutputDataSize = sizeof(entrypoint);
#pragma warning(push)
#pragma warning(disable:4996)
        hr = pVContext->DecoderExtension(pDecoder, &ExtensionData);
#pragma warning(pop)
        if (FAILED(hr))
        {
            ReleaseDevice();
            return MFX_ERR_DEVICE_FAILED;
        }

        return MFX_ERR_NONE;
    }

    mfxStatus MfxDxLoader::GetEntryPoints(void* table, mfxU32 *tableSize)
    {
        if (!pDecoder || !pVContext)
            return MFX_ERR_NULL_PTR;

        struct _entryPoints {
            uint32_t tablesize;
            void* table;
        } entryPoints = { 0 , table };
        HRESULT hr = S_OK;

        D3D11_VIDEO_DECODER_EXTENSION ExtensionData = {};

        /* Query amount of entrypoints */
        ExtensionData.Function = 0x01020304;
        ExtensionData.pPrivateOutputData = table;
        ExtensionData.PrivateOutputDataSize = sizeof(entryPoints);
#pragma warning(push)
#pragma warning(disable:4996)
        hr = pVContext->DecoderExtension(pDecoder, &ExtensionData);
#pragma warning(pop)
        if (FAILED(hr))
        {
            ReleaseDevice();
            return MFX_ERR_DEVICE_FAILED;
        }

        *tableSize = entryPoints.tablesize;

        return MFX_ERR_NONE;
    }

    void MfxDxLoader::ReleaseDevice(void)
    {
        if (pDecoder)
        {
            pDecoder->Release();
            pDecoder = nullptr;
        }
        if (pVContext)
        {
            pVContext->Release();
            pVContext = nullptr;
        }
        if (pVDevice)
        {
            pVDevice->Release();
            pVDevice = nullptr;
        }
        if (pContext)
        {
            pContext->Release();
            pContext = nullptr;
        }
        if (pDevice)
        {
            pDevice->Release();
            pDevice = nullptr;
        }
        if (pAdapter)
        {
            pAdapter->Release();
            pAdapter = nullptr;
        }
    }


    mfxStatus MfxDxLoader::FindAdapter(const mfxU32 adapterNum)
    {
        IDXGIFactory1 *pFactory = NULL;
        DXGI_ADAPTER_DESC1 desc = { 0 };
        mfxU32 curAdapter = 0;
        HRESULT hRes = E_FAIL;

#ifndef MEDIASDK_UWP_LOADER
        typedef
            HRESULT(WINAPI *DXGICreateFactoryFunc) (REFIID riid, void **ppFactory);
        HMODULE dxgiMod = NULL;
        DXGICreateFactoryFunc pFunc = NULL;
        dxgiMod = LoadLibraryEx(L"dxgi.dll", NULL, 0);
        if (!dxgiMod)
        {
            return MFX_ERR_UNSUPPORTED;
        }

        // load address of procedure to create DXGI 1.1 factory
        pFunc = (DXGICreateFactoryFunc)GetProcAddress(dxgiMod, "CreateDXGIFactory1");
        if (NULL == pFunc)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        hRes = pFunc(__uuidof(IDXGIFactory1), (void**)(&pFactory));
#else
        hRes = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&pFactory));
#endif

        if (FAILED(hRes))
        {
            return MFX_ERR_UNSUPPORTED;
        }

        curAdapter = 0;
        do
        {
            // get the required adapted
            hRes = pFactory->EnumAdapters1(curAdapter, &pAdapter);
            if (FAILED(hRes))
            {
                break;
            }

            // if it is the required adapter, save the interface
            if (curAdapter == adapterNum)
            {
                break;
            }
            else
            {
                pAdapter->Release();
                pAdapter = NULL;
            }

            // get the next adapter
            curAdapter += 1;
        } while (SUCCEEDED(hRes));

        if (pAdapter)
        {
            hRes = pAdapter->GetDesc1(&desc);
            if (desc.VendorId == INTEL_VENDOR_ID)
            {
                return MFX_ERR_NONE;
            }
            else
            {
                pAdapter->Release();
                pAdapter = NULL;
            }
        }

        return MFX_ERR_NOT_FOUND;
    }

    mfxStatus MfxDxLoader::Init(const mfxU32 adapterNum)
    {
        mfxStatus sts = MFX_ERR_NONE;

        sts = FindAdapter(adapterNum);

        if (sts != MFX_ERR_NONE) return sts;

        sts = InitDevice();

        if (sts != MFX_ERR_NONE) return sts;

        sts = InitDecoder();

        if (sts != MFX_ERR_NONE) return sts;

        return MFX_ERR_NONE;
    }
} //namespace MFX

#endif // !defined(OPEN_SOURCE) && defined(MEDIASDK_DX_LOADER)
