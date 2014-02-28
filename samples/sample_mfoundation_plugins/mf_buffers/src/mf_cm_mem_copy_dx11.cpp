/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mf_utils.h"

#ifdef CM_COPY_RESOURCE

#define CM_DX11
#include <cm_rt.h>

#include <mf_cm_mem_copy.h>
#ifdef  ENABLE_CM_COPY_D3D11

#ifndef WIN64
    #pragma comment (lib, "igfx11cmrt32.lib")
#else
    #pragma comment (lib, "igfx11cmrt64.lib")
#endif
CmDeviceHelper<ID3D11Device> :: CmDeviceHelper(const CComPtr<ID3D11Device> & rdevice)
    : base(rdevice)
{
    mfxU32 cmVersion = 0;
    INT cmSts = ::CreateCmDevice(m_pCmDevice, cmVersion, rdevice);

    if (CM_SUCCESS != cmSts)
        return ;

    if (CM_1_0 > cmVersion)
    {
        DestroyCmDevice(m_pCmDevice);
        m_pCmDevice = NULL;
        return ;
    }

    cmSts = m_pCmDevice->CreateQueue(m_pQueue);
    if (CM_SUCCESS != cmSts)
    {
        DestroyCmDevice(m_pCmDevice);
        m_pCmDevice = NULL;
    }
}

CmDeviceHelper<ID3D11Device> :: ~CmDeviceHelper()
{
    if (NULL == m_pCmDevice)
        return;
    DestroyCmDevice(m_pCmDevice);
}

mfxStatus CmDeviceHelper<ID3D11Device> :: CreateCmSurface(CmSurfaces_t & cmSurfaces, const CComPtr<Surface_t> & dxSurface)
{
    if (!m_pCmDevice)
        return MFX_ERR_DEVICE_FAILED;

    D3D11_TEXTURE2D_DESC dsc = {0};
    dxSurface->GetDesc(&dsc);

    cmSurfaces.resize(dsc.ArraySize);

    UINT nSurfaces = 0;
    INT cm_sts = m_pCmDevice->CreateSurface2DSubresource(dxSurface, dsc.ArraySize, &cmSurfaces.front(), nSurfaces);
    
    //todo: handle cm ret_code
    if (CM_SUCCESS != cm_sts) {
        return MFX_ERR_UNKNOWN;
    }

    if (nSurfaces != dsc.ArraySize) {
        for (size_t j = 0; j < cmSurfaces.size(); j++) {
            m_pCmDevice->DestroySurface(cmSurfaces[j]);
        }
        cmSurfaces.resize(0);
        return MFX_ERR_UNKNOWN;
    }
    
    return MFX_ERR_NONE;
}

CmSurface2D* CmDeviceHelper<ID3D11Device> :: FindCmSurface(CmSurfaces_t & cmSurfaces, int subresourceIdx)
{
    for (size_t i = 0; i < cmSurfaces.size(); i++) {
        CmSurface2D * pCmSurface2D = cmSurfaces[i];
        UINT FirstArraySlice;
        UINT FirstMipSlice;
        if (CM_SUCCESS != pCmSurface2D->QuerySubresourceIndex(FirstArraySlice, FirstMipSlice)) {
            return NULL;
        }
        if (FirstArraySlice == subresourceIdx) {
            return pCmSurface2D;
        }
    }
    return NULL;
}

#endif // #ifdef  ENABLE_CM_COPY_D3D11

#endif //#ifdef CM_COPY_RESOURCE