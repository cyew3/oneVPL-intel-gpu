/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#if MFX_D3D11_SUPPORT

#include <initguid.h>
#include <dxgidebug.h>
#include <atlbase.h>

class DXGIDebugMemLeakCheck : public MFXSingleton<DXGIDebugMemLeakCheck>
{
    typedef HRESULT (WINAPI *DXGIGetDebugInterface) (
        const IID &riid,
        void **ppDebug
        );
public:
    DXGIDebugMemLeakCheck()
    {
    }
    ~DXGIDebugMemLeakCheck()
    {
        static const vm_char sDxgiDebugLib [] = VM_STRING("Dxgidebug.dll");
        static const char sDxgiDebuInterface [] = "DXGIGetDebugInterface";

        DXGIGetDebugInterface m_pGetdxgiDebug;
        CComPtr<IDXGIDebug> m_dxgidebug;
        CComPtr<IDXGIInfoQueue> m_infoQueue;

        HMODULE hdxgi = GetModuleHandle(sDxgiDebugLib);

        if (!hdxgi)
        {
            PipelineTrace((VM_STRING("ERROR: Cannot GetModuleHandle %s\n"), sDxgiDebugLib));
            return;
        }

        m_pGetdxgiDebug = (DXGIGetDebugInterface)GetProcAddress(GetModuleHandle(sDxgiDebugLib), sDxgiDebuInterface);
        if (!m_pGetdxgiDebug)
        {
            PipelineTrace((VM_STRING("ERROR: Cannot GetProcAdress ") STR_TOKEN() VM_STRING("\n"), sDxgiDebuInterface));
            return;
        }
        HRESULT hr = m_pGetdxgiDebug(__uuidof(IDXGIDebug), (void**)&m_dxgidebug);
        if (FAILED(hr))
        {
            PipelineTrace((VM_STRING("ERROR: Cannot DXGIGetDebugInterface(IDXGIDebug) 0x%x\n"), hr));
            return;
        }

        hr = m_pGetdxgiDebug(__uuidof(IDXGIInfoQueue), (void**)&m_infoQueue);
        if (FAILED(hr))
        {
            PipelineTrace((VM_STRING("ERROR: Cannot DXGIGetDebugInterface(IDXGIInfoQueue) 0x%x\n"), hr));
            return;
        }

        m_dxgidebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);

        UINT64 nMessages = m_infoQueue->GetNumStoredMessagesAllowedByRetrievalFilters(DXGI_DEBUG_ALL);
        std::vector <char> message;
        for (size_t i = 0; i < nMessages; i++)
        {
            SIZE_T msg_len;
            if (FAILED(hr = m_infoQueue->GetMessage(DXGI_DEBUG_ALL , i, NULL, &msg_len)))
            {
                PipelineTrace((VM_STRING("ERROR: IDXGIInfoQueue::GetMessage()= 0x%x\n"), hr));
                return;
            }
            
            message.resize(msg_len);
            DXGI_INFO_QUEUE_MESSAGE *pmsg = (DXGI_INFO_QUEUE_MESSAGE*)&message.front();

            if (FAILED(hr = m_infoQueue->GetMessage(DXGI_DEBUG_ALL , i, pmsg, &msg_len)))
            {
                PipelineTrace((VM_STRING("ERROR: IDXGIInfoQueue::GetMessage()= 0x%x\n"), hr));
                return;
            }
            if (0 == i)
            {
                PipelineTrace((VM_STRING("\n###############################################################################")));
                PipelineTrace((VM_STRING("\n### DXGIDebug report:\n")));
            }
            PipelineTrace((VM_STRING("### ") STR_TOKEN() VM_STRING("\n"), pmsg->pDescription));
        }
    }
};

#include "mfx_d3d11_device.h"

class MFXD3D11DxgiDebugDevice : public MFXD3D11Device
{

public:
    virtual HRESULT D3D11CreateDeviceWrapper(
        IDXGIAdapter* pAdapter,
        D3D_DRIVER_TYPE DriverType,
        HMODULE Software,
        UINT Flags,
        CONST D3D_FEATURE_LEVEL* pFeatureLevels,
        UINT FeatureLevels,
        UINT SDKVersion,
        ID3D11Device** ppDevice,
        D3D_FEATURE_LEVEL* pFeatureLevel,
        ID3D11DeviceContext** ppImmediateContext )
    {
        //create dxgidebug helper
        DXGIDebugMemLeakCheck::Instance();

        HRESULT hr = D3D11CreateDevice(pAdapter,
            DriverType,
            Software,
            Flags | D3D11_CREATE_DEVICE_DEBUG,
            pFeatureLevels,
            FeatureLevels,
            SDKVersion,
            ppDevice,
            pFeatureLevel,
            ppImmediateContext);
        return hr;
    }
};



#endif