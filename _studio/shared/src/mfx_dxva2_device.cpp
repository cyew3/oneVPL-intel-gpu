/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

File Name: mfx_dxva2_device.cpp

\* ****************************************************************************** */

#include "mfx_common.h"

#if defined(MFX_VA_WIN)

#include <iostream>
#include <sstream>

#define INITGUID
#include <dxgi.h>

#include "mfx_dxva2_device.h"


using namespace MFX;


DXDevice::DXDevice(void)
{
    m_hModule = (HMODULE) 0;

    m_numAdapters = 0;

    m_vendorID = 0;
    m_deviceID = 0;
    m_driverVersion = 0;
    m_luid = 0;

} // DXDevice::DXDevice(void)

DXDevice::~DXDevice(void)
{
    Close();

    // free DX library only when device is destroyed
    UnloadDLLModule();

} // DXDevice::~DXDevice(void)

mfxU32 DXDevice::GetVendorID(void) const
{
    return m_vendorID;

} // mfxU32 DXDevice::GetVendorID(void) const

mfxU32 DXDevice::GetDeviceID(void) const
{
    return m_deviceID;

} // mfxU32 DXDevice::GetDeviceID(void) const

mfxU64 DXDevice::GetDriverVersion(void) const
{
    return m_driverVersion;
    
}// mfxU64 DXDevice::GetDriverVersion(void) const

mfxU64 DXDevice::GetLUID(void) const
{
    return m_luid;

} // mfxU64 DXDevice::GetLUID(void) const

mfxU32 DXDevice::GetAdapterCount(void) const
{
    return m_numAdapters;

} // mfxU32 DXDevice::GetAdapterCount(void) const

void DXDevice::Close(void)
{
    m_numAdapters = 0;

    m_vendorID = 0;
    m_deviceID = 0;
    m_luid = 0;

} // void DXDevice::Close(void)

void DXDevice::LoadDLLModule(const wchar_t *pModuleName)
{
    UINT prevErrorMode;

    // unload the module if it is required
    UnloadDLLModule();

    // set the silent error mode
    prevErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);

    // load d3d9 library
    m_hModule = LoadLibraryExW(pModuleName, NULL, 0);

    // set the previous error mode
    SetErrorMode(prevErrorMode);

} // void LoadDLLModule(const wchar_t *pModuleName)

void DXDevice::UnloadDLLModule(void)
{
    if (m_hModule)
    {
        FreeLibrary(m_hModule);
        m_hModule = (HMODULE) 0;
    }

} // void DXDevice::UnloaDLLdModule(void)

typedef
HRESULT (WINAPI *DXGICreateFactoryFunc) (REFIID riid, void **ppFactory);

DXGI1Device::DXGI1Device(void)
{
    m_pDXGIFactory1 = (void *) 0;
    m_pDXGIAdapter1 = (void *) 0;

} // DXGI1Device::DXGI1Device(void)

DXGI1Device::~DXGI1Device(void)
{
    Close();

} // DXGI1Device::~DXGI1Device(void)

void DXGI1Device::Close(void)
{
    // release the interfaces
    if (m_pDXGIAdapter1)
    {
        ((IDXGIAdapter1 *) m_pDXGIAdapter1)->Release();
    }

    if (m_pDXGIFactory1)
    {
        ((IDXGIFactory1 *) m_pDXGIFactory1)->Release();
    }

    m_pDXGIFactory1 = (void *) 0;
    m_pDXGIAdapter1 = (void *) 0;

} // void DXGI1Device::Close(void)

bool DXGI1Device::Init(const mfxU32 adapterNum)
{
    // release the object before initialization
    Close();

    // load up the library if it is not loaded
    if (NULL == m_hModule)
    {
        LoadDLLModule(L"dxgi.dll");
    }

    if (m_hModule)
    {
        DXGICreateFactoryFunc pFunc;
        IDXGIFactory1 *pFactory;
        IDXGIAdapter1 *pAdapter;
        DXGI_ADAPTER_DESC1 desc;
        mfxU32 curAdapter, maxAdapters;
        HRESULT hRes;

        // load address of procedure to create DXGI factory 1
        pFunc = (DXGICreateFactoryFunc) GetProcAddress(m_hModule, "CreateDXGIFactory1");
        if (NULL == pFunc)
        {
            return false;
        }

        // create the factory
        hRes = pFunc(__uuidof(IDXGIFactory1), (void**) (&pFactory));
        if (FAILED(hRes))
        {
            return false;
        }
        m_pDXGIFactory1 = pFactory;

        // get the number of adapters
        curAdapter = 0;
        maxAdapters = 0;
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
                m_pDXGIAdapter1 = pAdapter;
            }
            else
            {
                pAdapter->Release();
            }

            // get the next adapter
            curAdapter += 1;

        } while (SUCCEEDED(hRes));
        maxAdapters = curAdapter;

        // there is no required adapter
        if (adapterNum >= maxAdapters)
        {
            return false;
        }
        pAdapter = (IDXGIAdapter1 *) m_pDXGIAdapter1;

        // get the adapter's parameters
        hRes = pAdapter->GetDesc1(&desc);
        if (FAILED(hRes))
        {
            return false;
        }

        // save the parameters
        m_vendorID = desc.VendorId;
        m_deviceID = desc.DeviceId;
        *((LUID *) &m_luid) = desc.AdapterLuid;
    }

    return true;

} // bool DXGI1Device::Init(const mfxU32 adapterNum)

DXVA2Device::DXVA2Device(void)
{
    m_numAdapters = 0;
    m_vendorID = 0;
    m_deviceID = 0;
    m_driverVersion = 0;
} // DXVA2Device::DXVA2Device(void)

DXVA2Device::~DXVA2Device(void)
{
    Close();

} // DXVA2Device::~DXVA2Device(void)

void DXVA2Device::Close(void)
{
    m_numAdapters = 0;
    m_vendorID = 0;
    m_deviceID = 0;
    m_driverVersion = 0;
} // void DXVA2Device::Close(void)

bool DXVA2Device::InitDXGI1(const mfxU32 adapterNum)
{
    DXGI1Device dxgi1Device;
    bool bRes;

    // release the object before initialization
    Close();

    // create modern DXGI device
    bRes = dxgi1Device.Init(adapterNum);
    if (false == bRes)
    {
        return false;
    }

    // save the parameters and ...
    m_vendorID = dxgi1Device.GetVendorID();
    m_deviceID = dxgi1Device.GetDeviceID();
    m_numAdapters = dxgi1Device.GetAdapterCount();

    // ... say goodbye
    return true;

} // bool DXVA2Device::InitDXGI1(const mfxU32 adapterNum)

mfxU32 DXVA2Device::GetVendorID(void) const
{
    return m_vendorID;

} // mfxU32 DXVA2Device::GetVendorID(void) const

mfxU32 DXVA2Device::GetDeviceID(void) const
{
    return m_deviceID;

} // mfxU32 DXVA2Device::GetDeviceID(void) const

mfxU64 DXVA2Device::GetDriverVersion(void) const
{
    return m_driverVersion;
}// mfxU64 DXVA2Device::GetDriverVersion(void) const

mfxU32 DXVA2Device::GetAdapterCount(void) const
{
    return m_numAdapters;

} // mfxU32 DXVA2Device::GetAdapterCount(void) const
#elif defined(MFX_VA_LINUX) || defined(MFX_VA_OSX)
#include "mfx_dxva2_device.h"
namespace MFX
{
    DXDevice::DXDevice(void) {}
    DXDevice::~DXDevice(void) {}

    // Obtain graphic card's parameter
    mfxU32 DXDevice::GetVendorID(void) const {return 0;}
    mfxU32 DXDevice::GetDeviceID(void) const {return 0;}
    mfxU64 DXDevice::GetDriverVersion(void) const { return 0; }
    mfxU64 DXDevice::GetLUID(void) const { return 0; }
    mfxU32 DXDevice::GetAdapterCount(void) const { return 0; }
    void DXDevice::LoadDLLModule(const wchar_t *pModuleName) { pModuleName; return; }
    
    DXVA2Device::DXVA2Device(void) {}   
    DXVA2Device::~DXVA2Device(void) {}
    bool DXVA2Device::InitDXGI1(const mfxU32 adapterNum) { adapterNum; return true; }
    mfxU32 DXVA2Device::GetVendorID(void) const {return 0;}  
    mfxU32 DXVA2Device::GetDeviceID(void) const {return 0;} 
    mfxU64 DXVA2Device::GetDriverVersion(void) const {return 0;} 
    mfxU32 DXVA2Device::GetAdapterCount(void) const {return 1;} 
    void DXVA2Device::Close(void) { }
   
    DXGI1Device::DXGI1Device(void) {}
    DXGI1Device::~DXGI1Device(void) {}
    bool DXGI1Device::Init(const mfxU32 adapterNum) { adapterNum; return true; }
    void DXGI1Device::Close(void) {}
};
#endif // #if defined(MFX_VA_WIN)
