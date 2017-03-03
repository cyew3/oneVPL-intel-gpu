/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#if MFX_D3D11_SUPPORT


#include "mf_dx11_support.h"

MFDeviceD3D11::MFDeviceD3D11()
    : m_pDXGIManager()
    , m_DeviceResetToken()
    , m_pD3D11Device()
{
}

MFDeviceD3D11::~MFDeviceD3D11()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    DXVASupportClose();
}

HRESULT MFDeviceD3D11::DXVASupportInit (void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = CreateDXGIManagerAndDevice();
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

void    MFDeviceD3D11::DXVASupportClose(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    SAFE_RELEASE(m_pDXGIManager);
    SAFE_RELEASE(m_pD3D11Device);
}

HRESULT MFDeviceD3D11::DXVASupportInitWrapper (IMFDeviceDXVA* pDeviceDXVA)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_S(MF_TL_GENERAL, "Unsupported");

    pDeviceDXVA;

    return E_FAIL;
}

void    MFDeviceD3D11::DXVASupportCloseWrapper(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_S(MF_TL_GENERAL, "Unsupported");
}

IUnknown* MFDeviceD3D11::GetDeviceManager (void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    IUnknown* pD3D11Device = NULL;

    if (NULL != m_pD3D11Device)
    {
        m_pD3D11Device->AddRef();
        pD3D11Device = m_pD3D11Device;
    }
    MFX_LTRACE_P(MF_TL_GENERAL, pD3D11Device);
    return pD3D11Device;
}

HRESULT MFDeviceD3D11::CreateDXGIManagerAndDevice()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    CHECK_EXPRESSION(NULL == m_pD3D11Device, E_FAIL); //need to call DXVASupportClose to release the device
    CHECK_EXPRESSION(NULL == m_pDXGIManager, E_FAIL); //need to call DXVASupportClose to release the manager
    HRESULT hr = S_OK;

    CComQIPtr<ID3D10Multithread> pMultiThread;
    CComPtr<ID3D11DeviceContext>    pD3DImmediateContext;

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    D3D_FEATURE_LEVEL featureLevel;
    UINT resetToken;

    do
    {
        for (DWORD dwCount = 0; dwCount < ARRAYSIZE(featureLevels); dwCount++)
        {
            hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevels[dwCount], 1, D3D11_SDK_VERSION, &m_pD3D11Device, &featureLevel, NULL);
            if (SUCCEEDED(hr))
            {
                CComQIPtr<ID3D11VideoDevice> pDX11VideoDevice (m_pD3D11Device);

                if (NULL != pDX11VideoDevice.p)
                {
                    MFX_LTRACE_P(MF_TL_GENERAL, pDX11VideoDevice.p);
                    std::vector<std::string > sFeatures;
                    sFeatures.push_back("D3D_FEATURE_LEVEL_11_1");
                    sFeatures.push_back("D3D_FEATURE_LEVEL_11_0");
                    sFeatures.push_back("D3D_FEATURE_LEVEL_10_1");
                    sFeatures.push_back("D3D_FEATURE_LEVEL_10_0");
                    sFeatures.push_back("D3D_FEATURE_LEVEL_9_3");
                    sFeatures.push_back("D3D_FEATURE_LEVEL_9_2");
                    sFeatures.push_back("D3D_FEATURE_LEVEL_9_1");
                    MFX_LTRACE_1(MF_TL_GENERAL, "Max feature Level=", "%s", sFeatures[dwCount].c_str());
                    break;
                }
                SAFE_RELEASE(m_pD3D11Device);
            }
        }

        if(FAILED(hr))
        {
            MFX_LTRACE_1(MF_TL_GENERAL, "D3D11CreateDevice failed hr=", "%d", hr)
            break;
        }

        if(NULL == m_pDXGIManager)
        {
            hr = MFCreateDXGIDeviceManager(&resetToken, &m_pDXGIManager);
            if(FAILED(hr))
            {
                break;
            }
            m_DeviceResetToken = resetToken;
        }
        hr = m_pDXGIManager->ResetDevice(m_pD3D11Device, m_DeviceResetToken);
        if(FAILED(hr))
        {
            MFX_LTRACE_1(MF_TL_GENERAL, "m_pDXGIManager->ResetDevice failed hr=", "%d", hr)
            break;
        }

        // Need to explicitly set the multithreaded mode for this device
        pMultiThread = m_pD3D11Device;
        if(NULL == pMultiThread.p)
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "pD3DImmediateContext->QueryInterface(ID3D10Multithread) failed ");
            break;
        }

        pMultiThread->SetMultithreadProtected(TRUE);

    }while(false);

    return hr;
}

#endif//MFX_D3D11_SUPPORT