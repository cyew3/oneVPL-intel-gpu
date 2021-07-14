// Copyright (c) 2007-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined(MFX_VA_WIN)

#include <iostream>
#include <sstream>

#define INITGUID
#include <dxgi.h>

#include "mfx_dxva2_device.h"

#include <atlbase.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <regex>

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
}

mfxU32 DXDevice::GetDeviceID(void) const
{
    return m_deviceID;
}

mfxU32 DXDevice::GetSubSysId() const
{
    return m_subSysId;
}

mfxU32 DXDevice::GetRevision() const
{
    return m_revision;
}

mfxU64 DXDevice::GetDriverVersion(void) const
{
    return m_driverVersion;
}

mfxU64 DXDevice::GetLUID(void) const
{
    return m_luid;
}

mfxU32 DXDevice::GetAdapterCount(void) const
{
    return m_numAdapters;
}

void DXDevice::Close(void)
{
    m_numAdapters = 0;

    m_vendorID = 0;
    m_deviceID = 0;
    m_luid = 0;

}

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

    typedef
        HRESULT (WINAPI *DXGICreateFactoryFunc) (REFIID riid, void **ppFactory);

    if (m_hModule)
    {
        IDXGIFactory1 *pFactory;
        IDXGIAdapter1 *pAdapter;
        mfxU32 curAdapter, maxAdapters;
        HRESULT hRes;

        // load address of procedure to create DXGI factory 1
        DXGICreateFactoryFunc pFunc = (DXGICreateFactoryFunc) GetProcAddress(m_hModule, "CreateDXGIFactory1");
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
        DXGI_ADAPTER_DESC1 desc;
        hRes = pAdapter->GetDesc1(&desc);
        if (FAILED(hRes))
        {
            return false;
        }

        // save the parameters
        m_vendorID = desc.VendorId;
        m_deviceID = desc.DeviceId;
        m_subSysId = desc.SubSysId;
        m_revision = desc.Revision;

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
}

mfxU32 DXVA2Device::GetDeviceID(void) const
{
    return m_deviceID;
}

mfxU64 DXVA2Device::GetDriverVersion(void) const
{
    return m_driverVersion;
}

mfxU32 DXVA2Device::GetAdapterCount(void) const
{
    return m_numAdapters;

}
#if defined(MFX_VA_WIN)
//Returns array of all registered (in DriverStore) RTs
namespace MFX
{
    std::vector<std::tuple<
        std::wstring /*path*/, mfxU32 /*deviceId*/, mfxU32 /*subSysId*/, mfxU32 /*revision*/
    > > QueryRegisteredRuntimes()
    {
        std::vector<std::tuple<
            std::wstring /*path*/, mfxU32 /*deviceId*/, mfxU32 /*subSysId*/, mfxU32 /*revision*/
        > > instances;

        std::vector<wchar_t> filter(40);
        StringFromGUID2(GUID_DEVCLASS_DISPLAY, filter.data(), int(filter.size()));

        ULONG size;
        if (CM_Get_Device_ID_List_SizeW(&size, filter.data(), CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT) != CR_SUCCESS)
            return instances;

        std::vector<wchar_t> list(size);
        if (CM_Get_Device_ID_List(filter.data(), list.data(), ULONG(list.size()), CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT) != CR_SUCCESS)
            return instances;

        //copy the full sequence of null-terminated strings
        std::wistringstream ss(std::wstring(list.data(), list.data() + list.size()));

        /*
        * Identifier must be in the form of PCI\\VEN_v(4)&DEV_d(4)&SUBSYS_s(4)n(4)&REV_r(2)\<Instance-Id>
        * v(4) - FOURCC of PCI SIG-assigned identifier for the device
        * d(4) - FOURCC of vendor's defined identifier for the device
        * s(4) - FOURCC of vendor's defined subsystem identifier
        * n(4) - FOURCC of PCI SIG-assigned identifier for the subsystem
        * r(2) - Two-characters revision's number
        * Instance-Id - unspecified string that identifies the device among other devices w/ the same type
        */
        std::wregex r(
            LR"x(PCI\\VEN_([[:xdigit:]]{4})&DEV_([[:xdigit:]]{4})&SUBSYS_([[:xdigit:]]{8})&REV_([[:xdigit:]]{2})\\.*)x",
            std::regex::icase
        );

        std::vector<wchar_t> id(list.size());
        while (ss.getline(id.data(), id.size(), L'\0'))
        {
            std::wcmatch m;
            if (!std::regex_match(id.data(), m, r)
                || m.size() < 5
                || wcstoul(m[1].str().c_str(), nullptr, 16) != 0x8086
            )
                continue;

            DEVINST instance;
            if (CM_Locate_DevNodeW(&instance, reinterpret_cast<DEVINSTID>(id.data()), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
                continue;

            CRegKey key;
            if (CM_Open_DevNode_Key(instance, KEY_READ, 0, RegDisposition_OpenExisting, &key.m_hKey, CM_REGISTRY_SOFTWARE) != CR_SUCCESS)
                continue;

            std::vector<wchar_t> path(1024);
            size = ULONG(path.size() * sizeof(std::wstring::value_type));
            key.QueryValue(L"DriverStorePathForVPL", nullptr, reinterpret_cast<BYTE*>(path.data()), &size);

            mfxU32 const deviceId = wcstoul(m[2].str().c_str(), nullptr, 16);
            mfxU32 const subSysId = wcstoul(m[3].str().c_str(), nullptr, 16);
            mfxU32 const revision = wcstoul(m[4].str().c_str(), nullptr, 16);

            instances.push_back(
                std::make_tuple(path.data(), deviceId, subSysId, revision)
            );
        }

        return std::move(instances);
    }
}
#endif //MFX_VA_WIN

#else
#include "mfx_dxva2_device.h"
namespace MFX
{
    DXDevice::DXDevice(void)
        : m_numAdapters(0)
        , m_vendorID(0)
        , m_deviceID(0)
        , m_subSysId(0)
        , m_revision(0)
        , m_driverVersion(0)
        , m_luid (0)
    {}
    DXDevice::~DXDevice(void) {}

    // Obtain graphic card's parameter
    mfxU32 DXDevice::GetVendorID() const { return 0; }
    mfxU32 DXDevice::GetDeviceID() const { return 0; }
    mfxU32 DXDevice::GetSubSysId() const { return 0; }
    mfxU32 DXDevice::GetRevision() const { return 0; }
    mfxU64 DXDevice::GetDriverVersion() const { return 0; }
    mfxU64 DXDevice::GetLUID() const { return 0; }
    mfxU32 DXDevice::GetAdapterCount() const { return 0; }
    void DXDevice::LoadDLLModule(const wchar_t * /*pModuleName*/) { return; }

    DXVA2Device::DXVA2Device(void)
        : m_numAdapters(0)
        , m_vendorID(0)
        , m_deviceID(0)
        , m_driverVersion(0)
    {}
    DXVA2Device::~DXVA2Device(void) {}
    bool DXVA2Device::InitDXGI1(const mfxU32 /*adapterNum*/) { return true; }
    mfxU32 DXVA2Device::GetVendorID(void) const {return 0;} 
    mfxU32 DXVA2Device::GetDeviceID(void) const {return 0;}
    mfxU64 DXVA2Device::GetDriverVersion(void) const {return 0;}
    mfxU32 DXVA2Device::GetAdapterCount(void) const {return 1;}
    void DXVA2Device::Close(void) { }
   
    DXGI1Device::DXGI1Device(void)
        : m_pDXGIFactory1(0)
        , m_pDXGIAdapter1(0)
    {}
    DXGI1Device::~DXGI1Device(void) {}
    bool DXGI1Device::Init(const mfxU32 /*adapterNum*/) { return true; }
    void DXGI1Device::Close(void) {}
};
#endif // #if defined(MFX_VA_WIN)
