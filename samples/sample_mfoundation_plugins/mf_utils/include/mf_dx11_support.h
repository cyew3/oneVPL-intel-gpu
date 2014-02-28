/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MF_DX11_SUPPORT_H__
#define __MF_DX11_SUPPORT_H__

#if MFX_D3D11_SUPPORT

#include "mf_dxva_support.h"
#include <d3d11.h>

class MFDeviceD3D11 : public MFDeviceBase
{
public:
    MFDeviceD3D11();
    virtual ~MFDeviceD3D11();
    
    // IMFDeviceDXVA methods
    virtual IUnknown* GetDeviceManager (void);
    virtual HRESULT DXVASupportInit (void);
    virtual void    DXVASupportClose(void);
    virtual HRESULT DXVASupportInitWrapper (IMFDeviceDXVA* pDeviceDXVA);
    virtual void    DXVASupportCloseWrapper(void);

protected:
    HRESULT                         CreateDXGIManagerAndDevice();
    ID3D11Device                    *m_pD3D11Device;
    IMFDXGIDeviceManager            *m_pDXGIManager;
    UINT                             m_DeviceResetToken;
};

#endif

#endif