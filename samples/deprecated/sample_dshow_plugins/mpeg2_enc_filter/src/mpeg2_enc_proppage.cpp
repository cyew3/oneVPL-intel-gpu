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

#include "afxwin.h"
#include "afxcmn.h"
#include "mpeg2_enc_proppage.h"
#include "current_date.h"
#include "mfx_filter_defs.h"

#include "mfx_filter_guid.h"


/////////////////////////////////////////////////////////////////////////////
// Property Page Class
/////////////////////////////////////////////////////////////////////////////

CMPEG2EncPropPage::CMPEG2EncPropPage(IUnknown *pUnk) :
CConfigPropPage(pUnk, IDD_PROPPAGE_CONFIGURE, IDS_PROPPAGE_CONFIGURE)
{
    m_pMPEG2EncProps    = NULL;
    InitDlgItemsArray();
}

CUnknown* WINAPI CMPEG2EncPropPage::CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr)
{
    CMPEG2EncPropPage *pNewObject = new CMPEG2EncPropPage(pUnk);
    if (pNewObject == NULL)
    {
        *pHr = E_OUTOFMEMORY;
    }

    return pNewObject;
}

HRESULT CMPEG2EncPropPage::OnConnect(IUnknown *pUnk)
{
    if (pUnk == NULL)
    {
        return E_POINTER;
    }

    ASSERT(m_pMPEG2EncProps == NULL);

    HRESULT hr = pUnk->QueryInterface(IID_IConfigureVideoEncoder, reinterpret_cast<void**>(&m_pMPEG2EncProps));
    CHECK_HRESULT(hr);
    MSDK_CHECK_POINTER(m_pMPEG2EncProps, E_FAIL);

    m_pMPEG2EncProps->GetParams(&m_EncoderParams);
    m_pEncProps = m_pMPEG2EncProps;

    return hr;
}

HRESULT CMPEG2EncPropPage::FillCombos(void)
{
    HRESULT hr = S_OK;
    const mfxU16 MaxItemsNum = 26;
    TCHAR*  strItems[MaxItemsNum];
    DWORD   nItemsData[MaxItemsNum];

    //encoder presets

    mfxU16 j = 0;
    //fill profile combo
    for (mfxU16 i = 0; i < CodecPreset::PresetsNum() && j < MaxItemsNum; i++)
    {
        if (CodecPreset::PRESET_LOW_LATENCY != i)
        {
            strItems[j]   = CodecPreset::Preset2Str(i);
            nItemsData[j] = i;
            j++;
        }
    }

    hr = FillCombo(IDC_COMBO_PROFILE, strItems, nItemsData, 4, 0);
    CHECK_HRESULT(hr);

    //fill throttle policy combo
    strItems[0] = _T("Auto Throttling");     nItemsData[0]  = 0;
    strItems[1] = _T("No Throttling");       nItemsData[1]  = 1;

    hr = FillCombo(IDC_COMBO_THROTTLE_POLICY, strItems, nItemsData, 2, 1);
    CHECK_HRESULT(hr);

    //fill bitrate combo
    strItems[0] = _T("192000");      nItemsData[0]  = 0;
    strItems[1] = _T("226000");      nItemsData[1]  = 1;

    hr = FillCombo(IDC_COMBO_BITRATE, strItems, nItemsData, 2, 0);
    CHECK_HRESULT(hr);

    //fill qp combo
    strItems[0] = _T("0");       nItemsData[0]  = 0;
    strItems[1] = _T("1");       nItemsData[1]  = 1;

    hr = FillCombo(IDC_COMBO_QP, strItems, nItemsData, 2, 0);
    CHECK_HRESULT(hr);

    //encoder params

    //fill profile2 combo
    strItems[0] = _T("Autoselect");  nItemsData[0] = 0;
    strItems[1] = _T("Simple");      nItemsData[1] = 5;
    strItems[2] = _T("Main");        nItemsData[2] = 4;
    strItems[3] = _T("SNR");         nItemsData[3] = 3;
    strItems[4] = _T("Spatial");     nItemsData[4] = 2;
    strItems[5] = _T("High");        nItemsData[5] = 1;

    hr = FillCombo(IDC_COMBO_PROFILE2, strItems, nItemsData, 6, m_EncoderParams.profile_idc);
    CHECK_HRESULT(hr);

    //fill level combo
    strItems[0]  = _T("Autoselect");    nItemsData[0]  = IConfigureVideoEncoder::Params::LL_AUTOSELECT;
    strItems[1]  = _T("Low");           nItemsData[1]  = IConfigureVideoEncoder::Params::LL_LOW;
    strItems[2]  = _T("Main");          nItemsData[2]  = IConfigureVideoEncoder::Params::LL_MAIN;
    strItems[3]  = _T("High1440");      nItemsData[3]  = IConfigureVideoEncoder::Params::LL_HIGH1440;
    strItems[4]  = _T("High");          nItemsData[4]  = IConfigureVideoEncoder::Params::LL_HIGH;

    hr = FillCombo(IDC_COMBO_LEVEl, strItems, nItemsData, 5, m_EncoderParams.level_idc);
    CHECK_HRESULT(hr);

    //fill RCMethod combo
    strItems[0] = _T("VBR");          nItemsData[0]  = 1;
    strItems[1] = _T("CBR");          nItemsData[1]  = 2;

    hr = FillCombo(IDC_COMBO_RCCONTROL, strItems, nItemsData, 2, m_EncoderParams.rc_control.rc_method);
    CHECK_HRESULT(hr);

    //fill Target Usage combo
    hr = FillTGUsageCombo(IDC_COMBO_TRGTUSE, m_EncoderParams.target_usage);
    CHECK_HRESULT(hr);

    return S_OK;
};

HRESULT CMPEG2EncPropPage::ConfigureEditsSpins(void)
{
    HRESULT hr = S_OK;

    //edit and spin for PS IDR
    ConfigureEditSpin(IDC_SPIN_IDR, 0, 15, m_EncoderParams.ps_control.GopPicSize, true);
    CHECK_HRESULT(hr);

    //edit and spin for PS I-Frame
    ConfigureEditSpin(IDC_SPIN_IFRAME, 1, 5, m_EncoderParams.ps_control.GopRefDist, true);
    CHECK_HRESULT(hr);

    //edit and spin for PS B-Frame
    ConfigureEditSpin(IDC_SPIN_BFRAME, 0, 961, m_EncoderParams.ps_control.BufferSizeInKB, true);
    CHECK_HRESULT(hr);

    //calculate slices number
    IConfigureVideoEncoder::Statistics statistics;
    DWORD nWidth(0), nHeight(0);

    m_pMPEG2EncProps->GetRunTimeStatistics(&statistics);
    nWidth  = statistics.width;
    nHeight = statistics.height;

    mfxU32 nMin(0), nMax(0), nRecommended(0);

    if (nWidth > 0)
    {
        nRecommended = CalculateDefaultBitrate(MFX_CODEC_MPEG2, m_EncoderParams.target_usage,
                                               m_EncoderParams.frame_control.width, m_EncoderParams.frame_control.height,
                                               1.0 * statistics.frame_rate / 100);
        nMax = nRecommended * 5;
        nMin = nRecommended / 5;
    }
    else
    {
        nMin         = 50;
        nMax         = 50 * 1000;
        nRecommended =  m_EncoderParams.rc_control.bitrate;
    }

    if (m_EncoderParams.rc_control.bitrate &&
        m_EncoderParams.rc_control.bitrate >= nMin &&
        m_EncoderParams.rc_control.bitrate <= nMax)
    {
        nRecommended = m_EncoderParams.rc_control.bitrate;
    }

    //edit and spin for RC Bitrate
    ConfigureEditSpin(IDC_SPIN_BITRATE, nMin, nMax, nRecommended, true);
    CHECK_HRESULT(hr);

    ConfigureEdit(IDC_EDIT_WIDTH, nWidth);
    ConfigureEdit(IDC_EDIT_HEIGHT, nHeight);
    return S_OK;
};

HRESULT CMPEG2EncPropPage::OnActivate(void)
{
    HRESULT hr = S_OK;

    if (NULL == m_pMPEG2EncProps)
    {
        return S_FALSE;
    }

    //fill all combo boxes
    hr = FillCombos();
    CHECK_HRESULT(hr);

    //fill all edits and spins
    hr = ConfigureEditsSpins();
    CHECK_HRESULT(hr);

    SetDlgItemData(IDC_COMBO_PRESET, m_EncoderParams.preset);

    UpdateDialogControls( MFX_CODEC_MPEG2
                        , IDC_COMBO_PROFILE
                        , IDC_COMBO_TRGTUSE
                        , IDC_EDIT_BITRATE
                        , IDC_SPIN_BITRATE);
    //EnableDialogControls();

    //return
    return S_OK;
}

void CMPEG2EncPropPage::InitDlgItemsArray()
{
    CConfigPropPage::ItemVal items[] =
    {
        {IDC_SPIN_BITRATE,    (DWORD*)0,                                            true},
        {IDC_SPIN_IFRAME,     (DWORD*)0,                                            true},

        {IDC_COMBO_PRESET,    (DWORD*)&m_EncoderParams.preset,                    false},

        //Target Usage
        {IDC_COMBO_TRGTUSE,   (DWORD*)&m_EncoderParams.target_usage,                false},

        //Profile
        {IDC_COMBO_PROFILE2,  (DWORD*)&m_EncoderParams.profile_idc,                 true},

        //Level
        {IDC_COMBO_LEVEl,     (DWORD*)&m_EncoderParams.level_idc,                   true},

        //RCControl
        {IDC_EDIT_BITRATE,    (DWORD*)&m_EncoderParams.rc_control.bitrate,          true},
        {IDC_COMBO_RCCONTROL, (DWORD*)&m_EncoderParams.rc_control.rc_method,        true},

        //PSMethod
        {IDC_EDIT_IDR,         (DWORD*)&m_EncoderParams.ps_control.GopPicSize,      true},
        {IDC_EDIT_IFRAME,      (DWORD*)&m_EncoderParams.ps_control.GopRefDist,      true},
        {IDC_EDIT_BFRAME,      (DWORD*)&m_EncoderParams.ps_control.BufferSizeInKB,  true},
        {IDC_EDIT_HEIGHT,     (DWORD*)&m_EncoderParams.frame_control.height,        false},
        {IDC_EDIT_WIDTH,      (DWORD*)&m_EncoderParams.frame_control.width,         false}
    };

    m_pItems = new CConfigPropPage::ItemVal[ARRAY_SIZE(items)];
    if (NULL == m_pItems)
    {
        return;
    }

    MSDK_MEMCPY_BUF(m_pItems, 0, ARRAY_SIZE(items) * sizeof (CConfigPropPage::ItemVal), items, sizeof (items));
    m_nItems = ARRAY_SIZE(items);
}

void CMPEG2EncPropPage::LoadResults(mfxVideoParam* pMfxParams)
{
    CopyMFXToEncoderParams(&m_EncoderParams, pMfxParams);

    CConfigPropPage::LoadResults(pMfxParams);
}

HRESULT CMPEG2EncPropPage::OnDeactivate(void)
{
    SaveResults();
    return S_OK;
}

INT_PTR CMPEG2EncPropPage::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:

        if (LOWORD(wParam) == IDC_COMBO_PRESET)
        {
            GetDlgItemData(IDC_COMBO_PRESET, &m_EncoderParams.preset);

            UpdateDialogControls( MFX_CODEC_MPEG2
                                , IDC_COMBO_PROFILE
                                , IDC_COMBO_TRGTUSE
                                , IDC_EDIT_BITRATE
                                , IDC_SPIN_BITRATE);
        }
        else
        if (LOWORD(wParam) == IDC_COMBO_RCCONTROL)
        {
            UpdateRateControls();
        }
        else
        if (LOWORD(wParam) == IDC_COMBO_PROFILE2)
        {
            UpdateProfileControls();
        }
        else
        if (LOWORD(wParam) == IDC_EDIT_BITRATE)
        {
            CSpinButtonCtrl* pcwndSpin;
            HWND             hWnd;
            CEdit*           pEdt;

            //get requested control
            hWnd = GetDlgItem(m_Dlg, IDC_SPIN_BITRATE);
            MSDK_CHECK_POINTER(hWnd, E_POINTER);

            pcwndSpin = (CSpinButtonCtrl*)CWnd::FromHandle(hWnd);
            MSDK_CHECK_POINTER(pcwndSpin, E_POINTER);

            //get requested control
            hWnd = GetDlgItem(m_Dlg, IDC_EDIT_BITRATE);
            MSDK_CHECK_POINTER(hWnd, E_POINTER);

            pEdt = (CEdit*)CWnd::FromHandle(hWnd);
            MSDK_CHECK_POINTER(pEdt, E_POINTER);

            int nLower(0), nUpper(0);
            pcwndSpin->GetRange32(nLower, nUpper);

            CString strBuffer;
            pEdt->GetWindowText(strBuffer);

            //in case of spin usage ',' is added for thousands.
            strBuffer.Remove(',');

            int nBitrate = _tstoi(strBuffer.GetBuffer());

            if (EN_KILLFOCUS == HIWORD(wParam) && ((nLower > nBitrate) || (nUpper < nBitrate)))
            {
                pcwndSpin->SetPos32(nLower);
            }
        }
        else
        if (LOWORD(wParam) == IDC_COMBO_TRGTUSE)
        {
            UpdateTGUsageControls( IDC_COMBO_TRGTUSE
                                 , IDC_EDIT_BITRATE
                                 , IDC_SPIN_BITRATE);
            //EnableDialogControls();
        }

        SetDirty();
        break;
    } // switch

    return CBasePropertyPage::OnReceiveMessage(hwnd,uMsg,wParam,lParam);
}

HRESULT CMPEG2EncPropPage::OnApplyChanges(void)
{
    HRESULT hr = S_OK;

    SaveResults();
    //save Params
    hr = m_pMPEG2EncProps->SetParams(&m_EncoderParams);

    if (FAILED(hr))
    {
        MessageBox(this->m_hwnd, _T("An error occured! Possible reasons: filter is not connected with other filters, MSDK library cannot be found or parameters configuration is unsupported."), 0, 0);
    }
    return S_OK;
}

HRESULT CMPEG2EncPropPage::OnDisconnect(void)
{
    if (m_pMPEG2EncProps)
    {
        m_pMPEG2EncProps->Release();
        m_pMPEG2EncProps = NULL;
    };

    CWnd::DeleteTempMap();

    return S_OK;
}