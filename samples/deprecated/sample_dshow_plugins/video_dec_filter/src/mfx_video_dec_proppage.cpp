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

#include "mfx_samples_config.h"

#include "mfx_video_dec_proppage.h"
#include "current_date.h"
#include "mfx_filter_defs.h"

#pragma warning( disable : 4748 )

/////////////////////////////////////////////////////////////////////////////
// Property Page Class
/////////////////////////////////////////////////////////////////////////////

CVideoDecPropPage::CVideoDecPropPage(IUnknown *pUnk) :
CBasePropertyPage(NAME("VideoProp"), pUnk, IDD_PROPPAGE_VIDEO, IDS_PROPPAGE_NAME)
{
    m_pVideoDecProps    = NULL;
}

CUnknown* WINAPI CVideoDecPropPage::CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr)
{
    CVideoDecPropPage *pNewObject = new CVideoDecPropPage(pUnk);
    if (pNewObject == NULL)
    {
        *pHr = E_OUTOFMEMORY;
    }

    return pNewObject;
}

HRESULT CVideoDecPropPage::OnConnect(IUnknown *pUnk)
{
    MSDK_CHECK_POINTER(pUnk, E_POINTER);
    return pUnk->QueryInterface(IID_IConfigureVideoDecoder, reinterpret_cast<void**>(&m_pVideoDecProps));
}

static const UINT_PTR IDT_STATUPDATER = 1;

HRESULT CVideoDecPropPage::OnActivate(void)
{
    MSDK_CHECK_POINTER(m_pVideoDecProps, S_FALSE);

    CAutoLock cObjectLock(&m_critSec);

    UINT_PTR uResult;

    uResult = SetTimer(m_hwnd, IDT_STATUPDATER, 100, (TIMERPROC)NULL);

    if (!uResult)
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT CVideoDecPropPage::OnDeactivate(void)
{
    CAutoLock cObjectLock(&m_critSec);

    KillTimer(m_hwnd, IDT_STATUPDATER);

    return S_OK;
}

INT_PTR CVideoDecPropPage::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_TIMER && wParam == IDT_STATUPDATER)
    {
        IConfigureVideoDecoder::Statistics stStats;

        HRESULT hr = S_OK;
        TCHAR strNumber[100];

        if (NULL == m_pVideoDecProps || NULL == m_Dlg)
        {
            return 1;
        }

        hr = m_pVideoDecProps->GetRunTimeStatistics(&stStats);

        if (S_OK != hr)
        {
            return 1;
        }

        _sntprintf(strNumber, 100, _T("%.2f"), stStats.decode_time / (double)1e2);
        SetDlgItemText(m_Dlg, IDC_EDIT_DecTime, strNumber);
        _sntprintf(strNumber, 100, _T("%d"), stStats.frames_decoded);
        SetDlgItemText(m_Dlg, IDC_EDIT_DecFrame, strNumber);
        _sntprintf(strNumber, 100, _T("%.2f"), stStats.frame_rate / (double)100);
        SetDlgItemText(m_Dlg, IDC_Framerate, strNumber);
        _sntprintf(strNumber, 100, _T("%d"), stStats.width);
        SetDlgItemText(m_Dlg, IDC_Width, strNumber);
        _sntprintf(strNumber, 100, _T("%d"), stStats.height);
        SetDlgItemText(m_Dlg, IDC_Height, strNumber);
    }

    return CBasePropertyPage::OnReceiveMessage(hwnd,uMsg,wParam,lParam);
}

HRESULT CVideoDecPropPage::OnApplyChanges(void)
{
    return S_OK;
}

HRESULT CVideoDecPropPage::OnDisconnect(void)
{
    if (m_pVideoDecProps)
    {
        m_pVideoDecProps->Release();
        m_pVideoDecProps = NULL;
    }

    return S_OK;
}


//////////////////////////////////////////////////////////////////////////
// About
//////////////////////////////////////////////////////////////////////////
CAboutPropPage::CAboutPropPage(IUnknown *pUnk) :
CBasePropertyPage(NAME("AboutProp"), pUnk, IDD_PROPPAGE_ABOUT, IDS_PROPPAGE_ABOUT)
{
    m_pAboutProps       = NULL;
}

CUnknown* WINAPI CAboutPropPage::CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr)
{
    CAboutPropPage *pNewObject = new CAboutPropPage(pUnk);
    if (pNewObject == NULL)
    {
        *pHr = E_OUTOFMEMORY;
    }

    return pNewObject;
}

HRESULT CAboutPropPage::OnConnect(IUnknown *pUnk)
{
    MSDK_CHECK_POINTER(pUnk, E_POINTER);
    return pUnk->QueryInterface(IID_IAboutPropertyPage, reinterpret_cast<void**>(&m_pAboutProps));
}

HRESULT CAboutPropPage::OnActivate(void)
{
    HRESULT hr = S_OK;

    MSDK_CHECK_POINTER(m_pAboutProps, S_FALSE);

    hr = m_pAboutProps->GetFilterName(&m_bstrFilterName);

    MSDK_CHECK_RESULT(hr, S_OK, S_FALSE);

    SetDlgItemText(m_Dlg, IDC_FilterName, m_bstrFilterName.Detach() );
    SetDlgItemText(m_Dlg, IDC_VERSION,    _T(FILE_VERSION_STRING) );

    const TCHAR* strProductVersion;
    strProductVersion = _tcschr(_T(PRODUCT_VERSION_STRING), _T(','));
    MSDK_CHECK_POINTER(strProductVersion, S_FALSE);
    strProductVersion = _tcschr(strProductVersion + 1, _T(','));
    MSDK_CHECK_POINTER(strProductVersion, S_FALSE);

    size_t nMFXVersionLen = _tcslen(_T(PRODUCT_VERSION_STRING))- _tcslen(strProductVersion);
    TCHAR *strMFXVersion = new TCHAR[nMFXVersionLen + 1];

    MSDK_MEMCPY_BUF(strMFXVersion, 0, nMFXVersionLen*sizeof(TCHAR), _T(PRODUCT_VERSION_STRING), nMFXVersionLen * sizeof(TCHAR));
    strMFXVersion[nMFXVersionLen] = 0;

    SetDlgItemText(m_Dlg, IDC_VERSION_MFX, strMFXVersion);

    size_t nStrCopyrightLen = _tcslen(_T(PRODUCT_COPYRIGHT));
    TCHAR *strCopyright = new TCHAR[nStrCopyrightLen + 1];
    MSDK_MEMCPY_BUF(strCopyright, 0, nStrCopyrightLen * sizeof(TCHAR), _T(PRODUCT_COPYRIGHT), nStrCopyrightLen * sizeof(TCHAR));
    strCopyright[nStrCopyrightLen] = 0;

    SetDlgItemText(m_Dlg, IDC_Copyright, strCopyright);
    SetDlgItemText(m_Dlg, IDC_URL, _T("http://www.intel.com"));

    delete[] strMFXVersion;
    delete[] strCopyright;

    return S_OK;
}

HRESULT CAboutPropPage::OnDisconnect(void)
{
    if (m_pAboutProps)
    {
        m_pAboutProps->Release();
        m_pAboutProps = NULL;
    };

    return S_OK;
};