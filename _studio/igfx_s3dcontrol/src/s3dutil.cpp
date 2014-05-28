//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.
//

#include <memory>
#include "private_query_device.h"

#include "igfxext.h"
#include "igfxext_i.c"
#include "comutil.h"
#include "S3DUtil.h"
//Static to prevent from being used in other files.
static ICUIExternal8 *pCUIExternal8;
//Constructor. Create the SDK COM object here
CS3DUtil::CS3DUtil(void)
{
    HRESULT hr = CoInitialize(NULL); //CoInitialize should be done only once in the thread
    //Get the SDK COm object
    hr = CoCreateInstance(CLSID_CUIExternal, NULL, CLSCTX_SERVER, IID_ICUIExternal8, (void **)&pCUIExternal8);
    if(FAILED(hr))
    {
        pCUIExternal8 = NULL;
    }
    m_bPlatformS3DSupport = false;
    m_dwDisplayUID = NULL;
    ZeroMemory(&m_S3DCaps, sizeof(IGFX_S3D_CAPS_STRUCT));
}
//Release the SDK object if it was created
CS3DUtil::~CS3DUtil(void)
{
    if(NULL != pCUIExternal8)
    {
        //Release the SDK COM object
        pCUIExternal8->Release();
        pCUIExternal8 = NULL;
    }
    //CoUninitialize should be done only once in the thread
    CoUninitialize();
}

/****************************************************************
 * FUNCTION NAME:   GetMonitorUid
 *
 * DESCRIPTION:     To get the device ID of a device
 *
 * PARAMETERS:      IN DWORD dwDeviceTypeReq
 *                                      IGFX_CRT
 *                                      IGFX_LocalFP
 *                                      IGFX_ExternalFP
 *                                      IGFX_TV
 *                                      IGFX_HDMI
 *                                      IGFX_NIVO
 *                                      IGFX_DP
 *                                  OUT DWORD* uidMonitorReq
 *
 * AUTHOR:          Sanish
 ****************************************************************/
bool CS3DUtil::GetMonitorUid(DWORD dwDeviceTypeReq, DWORD* uidMonitorReq)
{
    bool bret = false;
    DWORD uidMonitor = NULL;
    DWORD dwDeviceType = NULL;
    DWORD dwStatus = NULL;
    HRESULT hr = E_FAIL;
    int index = 0;
    int AdapterIndex = 0;
    _bstr_t bstrDev1 = _T("\\\\.\\Display1");
    _bstr_t bstrDev2 = _T("\\\\.\\Display2");
    _bstr_t bstrDev[2] = {bstrDev1, bstrDev2};

    //Check if the SDK interface is valid
    if(NULL != pCUIExternal8)
    {
        for(AdapterIndex = 0; AdapterIndex < 2 ; AdapterIndex++)
        {
            //Currently CUI supports 7 device types
            for(index = 0; index < 7 ; index++)
            {
                //The tweek to get the HDMI and DP support on this API
                if(0 == index)
                {
                    dwDeviceType = (DWORD)(IGFX_FLAG_HDMI_DEIVCE_SUPPORT | IGFX_FLAG_DP_DEVICE_SUPPORT);
                }
                hr = pCUIExternal8->EnumAttachableDevices(bstrDev[AdapterIndex], index, &uidMonitor,
                                                            &dwDeviceType, &dwStatus);

                if(SUCCEEDED(hr))
                {
                    //Check if the Device type matches and is it attached
                    if((dwDeviceTypeReq == dwDeviceType) && (IGFX_DISPLAY_DEVICE_NOTATTACHED != dwStatus))
                    {
                        //yes got the monitor ID of the required device type
                        *uidMonitorReq = uidMonitor;
                        bret = true;
                        break;
                    }
                }
            }
            //Break if we already got a valid devce ID
            if(NULL != *uidMonitorReq)
            {
                break;
            }
        }
        //Check if we got a valid device ID
        if(NULL == *uidMonitorReq)
        {
            //MessageBox("Requested Device Not Attached");
            //bret = false;
        }
    }
    return bret;
}
/****************************************************************
 * FUNCTION NAME:   GetPrimaryMonitorUid
 *
 * DESCRIPTION:     To get the device ID of the primary device
 *
 * PARAMETERS:      OUT DWORD* uidMonitorReq
 *
 * AUTHOR:          Sanish
 ****************************************************************/
bool CS3DUtil::GetPrimaryMonitorUid(DWORD* uidMonitorReq)
{
    bool bRet = false;
    if(NULL != pCUIExternal8)
    {
        IGFX_SYSTEM_CONFIG_DATA Config;
        DWORD dwsize = sizeof(Config);
        ZeroMemory(&Config, dwsize);
        *uidMonitorReq = NULL;
        DWORD dwError = 0;
        HRESULT hr = pCUIExternal8->GetDeviceData(IGFX_GET_SET_CONFIGURATION_GUID, dwsize, (BYTE*)&Config, &dwError);

        if(SUCCEEDED(hr) && (IGFX_SUCCESS == dwError))
        {
            bRet = true;
            //Add code if anything specific needs to be done based on the operating mode
            //if(IGFX_DISPLAY_DEVICE_CONFIG_FLAG_DDCLONE == Config.dwOpMode)

            //Copy the primary display ID
            *uidMonitorReq = Config.PriDispCfg.dwDisplayUID;
        }
    }
    return bRet;
}
/****************************************************************
 * FUNCTION NAME:   GetS3DCaps
 *
 * DESCRIPTION:     Gets the S3D caps on the HDMI device
 *
 * PARAMETERS:      None, just Gets S3D Caps on the attached HDMI device
 *
 * AUTHOR:          Sanish
 ****************************************************************/
bool CS3DUtil::GetS3DCaps()
{
    bool bRet = false;
    if(NULL != pCUIExternal8)
    {
        HRESULT hr = E_FAIL;
        //The S3D caps struct

        DWORD dwSize = sizeof(IGFX_S3D_CAPS_STRUCT);
        //initialize to 0
        ZeroMemory(&m_S3DCaps, dwSize);
        DWORD dwError = 0;
        //NULL device ID to get the platform support
        m_S3DCaps.dwDisplayUID = NULL;
        //First check whether the platform supports S3D
        hr = pCUIExternal8->GetDeviceData(IGFX_S3D_CAPS_GUID, dwSize, (BYTE*)&m_S3DCaps, &dwError);
        //Check if the call has succeeded
        if(SUCCEEDED(hr) && (IGFX_SUCCESS == dwError))
        {
            m_bPlatformS3DSupport = m_S3DCaps.bSupportsS3DLRFrames;
            //Check if the platform has the support.
            if(m_S3DCaps.bSupportsS3DLRFrames)
            {
                //Initialize to NULL
                m_dwDisplayUID = NULL;
                //get the device ID for the device.
                if(GetPrimaryMonitorUid(&m_dwDisplayUID))
                {
                    //Again clear the structure, and fill in the device ID
                    ZeroMemory(&m_S3DCaps, dwSize);
                    m_S3DCaps.dwDisplayUID = m_dwDisplayUID;
                    //Get the caps for the specific device
                    hr = pCUIExternal8->GetDeviceData(IGFX_S3D_CAPS_GUID, dwSize, (BYTE*)&m_S3DCaps, &dwError);
                    if(SUCCEEDED(hr) && (IGFX_SUCCESS == dwError))
                    {
                        //Just make sure we've got some valid modes
                        /*if(m_S3DCaps.bSupportsS3DLRFrames && m_S3DCaps.ulNumEntries > 0)
                        {
                            //Just loop thru the supported modes
                            int index = 0;
                            for(index = 0; index < m_S3DCaps.ulNumEntries; index ++)
                            {
                                //S3DCaps.S3DCapsPerMode[index].ulResHeight;
                            }
                        }*/
                        bRet = true;
                    }
                }
            }
        }
    }
    return bRet;
}
/****************************************************************
 * FUNCTION NAME:   SetS3DMode
 *
 * DESCRIPTION:     Enables\Disables the S3D mode on a particular resolution
 *
 * PARAMETERS:      IGFX_S3D_CONTROL_STRUCT the resolution and device details
 *
 * AUTHOR:          Sanish
 ****************************************************************/
bool CS3DUtil::SetS3DMode(IGFX_S3D_CONTROL_STRUCT S3DControl)
{
    bool bRet = false;
    if(NULL != pCUIExternal8)
    {
        HRESULT hr = E_FAIL;
        DWORD dwError = 0;
        //Fill in the Device ID
        S3DControl.dwDisplayUID = m_dwDisplayUID;
        //check if the call is with FFFF, then set to 0 to pick the current mode
        if(S3DControl.ulResWidth == 0xFFFF)
        {
            S3DControl.ulResWidth = 0;
            S3DControl.ulResHeight = 0;
            S3DControl.ulRefreshRate = 0;
        }
        //Now just apply S3D on the first mode.
        hr = pCUIExternal8->SetDeviceData(IGFX_S3D_CONTROL_GUID, sizeof(IGFX_S3D_CONTROL_STRUCT), (BYTE*)&S3DControl, &dwError);
        //if(SUCCEEDED(hr) &&  (IGFX_SUCCESS == dwError)) //THis shoule be the bare minimum check  for a pass case
        if(SUCCEEDED(hr))
        {
            switch(dwError)//Notify the error codes
            {
            case IGFX_SUCCESS:
                bRet = true;
                break;
            case IGFX_S3D_ALREADY_IN_USE_BY_ANOTHER_PROCESS: //No two process can enable s3d at a time
                //MessageBox(NULL, _T("Already in use by another process, continue in 2D"), _T("Status"), MB_OK);
                break;
            case IGFX_S3D_DEVICE_NOT_PRIMARY: //As of now, S3D is blocked on secondary
                //MessageBox(NULL, _T("Monitor not primary, continue in 2D"), _T("Status"), MB_OK);
                break;
            case IGFX_S3D_INVALID_MODE_FORMAT:
                //MessageBox(NULL, _T("Invalid Mode or Format"), _T("Status"), MB_OK);
                break;
            case IGFX_S3D_INVALID_MONITOR_ID:
                //MessageBox(NULL, _T("Invalid device ID"), _T("Status"), MB_OK);
                break;

            //default:
                //MessageBox(NULL, _T("Unknown error"), _T("Status"), MB_OK);
            }
        }
    }
    else
    {
        //MessageBox(NULL, _T("Unknown error"), _T("Status"), MB_OK);
    }
    return bRet;
}
/****************************************************************
 * FUNCTION NAME:   TestS3D
 *
 * DESCRIPTION:     Just a sample code to show how to use it
 *
 * PARAMETERS:      None, just Gets and sets S3D on the attached HDMI device
 *
 * AUTHOR:          Sanish
 ****************************************************************/
bool CS3DUtil::TestS3D()
{
    bool bRet = false;
    if(NULL != pCUIExternal8)
    {
        HRESULT hr = E_FAIL;
        DWORD dwError = 0;
        //Get the S3D caps
        bool bS3DSupportedDevice = GetS3DCaps();
        if(bS3DSupportedDevice)
        {
            //Now just apply S3D on the first mode.
            IGFX_S3D_CONTROL_STRUCT S3DControl;
            ZeroMemory(&S3DControl, sizeof(IGFX_S3D_CONTROL_STRUCT));
            S3DControl.dwDisplayUID = m_dwDisplayUID;
            //Fill in the process ID, hard coding for the time being. the call will be failed if set to NULL
            S3DControl.dwProcessID = 1234;
            S3DControl.ulResWidth = m_S3DCaps.S3DCapsPerMode[0].ulResWidth;
            S3DControl.ulResHeight = m_S3DCaps.S3DCapsPerMode[0].ulResHeight;
            S3DControl.ulRefreshRate = m_S3DCaps.S3DCapsPerMode[0].ulRefreshRate;
            S3DControl.bEnable = true;
            //Pick one S3D format from the bit masks of S3DCaps.S3DCapsPerMode[0].dwMonitorS3DFormats.
            S3DControl.eS3DFormat =IGFX_eS3DFramePacking; //current support is only for IGFX_eS3DFramePacking
            hr = pCUIExternal8->SetDeviceData(IGFX_S3D_CONTROL_GUID, sizeof(IGFX_S3D_CONTROL_STRUCT), (BYTE*)&S3DControl, &dwError);
            if(SUCCEEDED(hr) && (IGFX_SUCCESS == dwError))
            {
                //Yes the call succeeded :)
                bRet = true;
                //Once done with 3D, get the system out of S3D.
                ZeroMemory(&S3DControl, sizeof(IGFX_S3D_CONTROL_STRUCT));
                S3DControl.dwDisplayUID = m_dwDisplayUID;
                //Fill in the process ID
                S3DControl.dwProcessID = 123;
                S3DControl.ulResWidth = 0;
                S3DControl.ulResHeight = 0;
                S3DControl.ulRefreshRate = 0;
                S3DControl.bEnable = false;
                //S3D disable call.
                hr = pCUIExternal8->SetDeviceData(IGFX_S3D_CONTROL_GUID, sizeof(IGFX_S3D_CONTROL_STRUCT), (BYTE*)&S3DControl, &dwError);
            }
        }
    }
    return bRet;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  MFXS3DWrapper implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT MFXS3DWrapper::SetDevice(IDirect3DDeviceManager9 *pDeviceManager)
{
    if (!pDeviceManager)
        return E_POINTER;

    HANDLE directXHandle = 0;
    IDirect3DDevice9* pD3DDevice = 0;
    try
    {
        bool bS3DSupportedDevice = m_utilS3D.GetS3DCaps();

        if (!bS3DSupportedDevice)
            return E_NOINTERFACE;

        HRESULT hr = pDeviceManager->OpenDeviceHandle(&directXHandle);
        if (FAILED(hr))
        {
            return E_FAIL;
        }

        hr = pDeviceManager->LockDevice(directXHandle, &pD3DDevice, true);
        if (FAILED(hr))
        {
            pDeviceManager->CloseDeviceHandle(directXHandle);
            return E_FAIL;
        }

        m_privateDevice.reset(new CPrivateQueryDevice(pD3DDevice));

        pD3DDevice->Release(); // why needed?
        pDeviceManager->UnlockDevice(directXHandle, true);
        pDeviceManager->CloseDeviceHandle(directXHandle);

        if (!m_privateDevice.get())
            return E_FAIL;
    }
    catch(...)
    {
        return E_UNEXPECTED;
    }

    return S_OK;
}

HRESULT MFXS3DWrapper::GetS3DCaps( IGFX_S3DCAPS *pCaps )
{
    if (!pCaps)
        return E_POINTER;

    bool ret = m_utilS3D.GetS3DCaps();

    if (ret)
    {
        pCaps->ulNumEntries = m_utilS3D.m_S3DCaps.ulNumEntries;

        for (UINT i = 0; i < max(m_utilS3D.m_S3DCaps.ulNumEntries, IGFX_MAX_S3D_MODES); i++)
        {
            memset(&(pCaps->S3DSupportedModes[i]), 0, sizeof(pCaps->S3DSupportedModes[i]));
            pCaps->S3DSupportedModes[i].ulRefreshRate = m_utilS3D.m_S3DCaps.S3DCapsPerMode[i].ulRefreshRate;
            pCaps->S3DSupportedModes[i].ulResWidth = m_utilS3D.m_S3DCaps.S3DCapsPerMode[i].ulResWidth;
            pCaps->S3DSupportedModes[i].ulResHeight = m_utilS3D.m_S3DCaps.S3DCapsPerMode[i].ulResHeight;
        }

        return S_OK;
    }
    else
    {
        return E_NOINTERFACE;
    }
}

HRESULT MFXS3DWrapper::SwitchToMode(BOOL isS3D, IGFX_DISPLAY_MODE *pMode /*= NULL*/)
{
    try
    {
        if (!m_utilS3D.GetS3DCaps())
            return E_NOINTERFACE;

        IGFX_S3D_CONTROL_STRUCT S3DControl;
        ZeroMemory(&S3DControl, sizeof(IGFX_S3D_CONTROL_STRUCT));

        S3DControl.dwProcessID   = GetCurrentProcessId();
        if (pMode)
        {
            S3DControl.ulResWidth    = pMode->ulResWidth;
            S3DControl.ulResHeight   = pMode->ulResHeight;
            S3DControl.ulRefreshRate = pMode->ulRefreshRate;
        }
        else
        {
            S3DControl.ulResWidth    = 0;
            S3DControl.ulResHeight   = 0;
            S3DControl.ulRefreshRate = 0;
        }

        S3DControl.bEnable = isS3D ? TRUE : FALSE;
        m_utilS3D.SetS3DMode(S3DControl);
    }
    catch(...)
    {
        return E_UNEXPECTED;
    }

    return S_OK;
}

HRESULT MFXS3DWrapper::SwitchTo3D(IGFX_DISPLAY_MODE *pMode)
{
    return SwitchToMode(TRUE, pMode);
}

HRESULT MFXS3DWrapper::SwitchTo2D(IGFX_DISPLAY_MODE *pMode)
{
    return SwitchToMode(FALSE, pMode);
}

HRESULT MFXS3DWrapper::SelectLeftView()
{
    return SelectView(TRUE);
}

HRESULT MFXS3DWrapper::SelectRightView()
{
    return SelectView(FALSE);
}

HRESULT MFXS3DWrapper::SelectView(BOOL isLeft)
{
    HRESULT hr = S_OK;

    if (!m_privateDevice.get())
        return E_FAIL;

    try
    {
        hr = m_privateDevice->SelectVppS3dMode(isLeft ? QUERY_S3D_MODE_L : QUERY_S3D_MODE_R);
    }
    catch(...)
    {
        return E_UNEXPECTED;
    }

    return hr;
}

IGFXS3DControl * CreateIGFXS3DControl()
{
    std::auto_ptr<MFXS3DWrapper> s3dWrapper(new MFXS3DWrapper());

    return s3dWrapper.release();
}

