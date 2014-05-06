/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2010 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include <stdio.h>
#include <d3d9.h>
#if MFX_D3D11_SUPPORT
	#include <d3d11.h>
	#pragma comment (lib, "d3d11.lib")
	#include "dx11_spy.h"
#endif

#include "src\detours.h"

//will link to spy component without including dxva2api header
#define DONT_USE_DXVA2API
#include "dxva2_spy.h"


#if !MFX_D3D11_SUPPORT
	//have to define these functions by ourself because of that calling convention used in dxva2.dll 
	//is stdcall 
	extern "C"__declspec(dllimport)
	HRESULT __stdcall DXVA2CreateDirect3DDeviceManager9(UINT* pResetToken,
                                                        IDirect3DDeviceManager9** ppDXVAManager);
#endif

//Pointers assignment to real function
static HRESULT (__stdcall * TrueCreateD3DDM)(
                UINT* pResetToken,
                IDirect3DDeviceManager9** ppDXVAManager) = DXVA2CreateDirect3DDeviceManager9;



//interception of calls is here
HRESULT __stdcall ModifiedCreateD3DDM(UINT* pResetToken,
                                      IDirect3DDeviceManager9** ppDXVAManager)
{
    HRESULT hr = TrueCreateD3DDM(pResetToken, ppDXVAManager);

    if (SUCCEEDED(hr))
    {
        //attaching interface spy
        char *env_buffer;
        size_t size;
        _dupenv_s(&env_buffer, &size, "dxva2_dump_dir");
        SetDumpingDirectory(env_buffer);
    }
    
    *ppDXVAManager = CreateDXVASpy(*ppDXVAManager);
    

    return hr;
}


#if MFX_D3D11_SUPPORT
static 
	HRESULT (WINAPI * TrueD3D11CreateDevice)(
    _In_opt_ IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    CONST D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    _Out_opt_ ID3D11Device** ppDevice,
    _Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
    _Out_opt_ ID3D11DeviceContext** ppImmediateContext ) = D3D11CreateDevice;

	HRESULT WINAPI ModifiedD3D11CreateDevice(
    _In_opt_ IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    CONST D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    _Out_opt_ ID3D11Device** ppDevice,
    _Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
    _Out_opt_ ID3D11DeviceContext** ppImmediateContext )
	{
		HRESULT hr = TrueD3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
		if (SUCCEEDED(hr))
		{
			//attaching interface spy
			char *env_buffer;
			size_t size;
			_dupenv_s(&env_buffer, &size, "dxva2_dump_dir");
			SetDumpingDirectory(env_buffer);
		}
		*ppDevice = new SpyID3D11Device(*ppDevice);

		printf("\nD3D11CreateDevice = 0xd (featurelevel = %d)\n", hr, pFeatureLevel ? *pFeatureLevel : 0);

		return hr;
	}

#endif

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    LONG error;
    (void)hinst;
    (void)reserved;
    
    if (dwReason == DLL_PROCESS_ATTACH) 
    {
        printf("dxva2_dump.dll: Starting.\n");
        fflush(stdout);

        DetourRestoreAfterWith();

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)TrueCreateD3DDM, ModifiedCreateD3DDM);
		DetourAttach(&(PVOID&)TrueD3D11CreateDevice, ModifiedD3D11CreateDevice);
        error = DetourTransactionCommit();

        if (error == NO_ERROR) 
        {
            printf("dxva2_dump.dll: Detoured DXVA2CreateDirect3DDeviceManager9().\n");
        }
        else 
        {
            printf("dxva2_dump.dll: Error detouring DXVA2CreateDirect3DDeviceManager9(): %d\n", error);
        }
    }
    else if (dwReason == DLL_PROCESS_DETACH) 
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)TrueCreateD3DDM, ModifiedCreateD3DDM);
		DetourDetach(&(PVOID&)TrueD3D11CreateDevice, ModifiedD3D11CreateDevice);
        error = DetourTransactionCommit();

        printf("dxva2_dump.dll: Removed DXVA2CreateDirect3DDeviceManager9() (result=%d)\n",error);
        fflush(stdout);
    }
    return TRUE;
}
