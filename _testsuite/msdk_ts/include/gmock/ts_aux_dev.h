//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64)

#pragma once

#include "d3d_device.h"
#include "d3d11_device.h"
#include "ts_common.h"

#define MFX_CHECK(EXPR, ERR)    { if (!(EXPR)) return (ERR); }

struct D3DAuxiliaryObjects
{
    HANDLE                          m_hRegistrationDevice;

    HANDLE                          m_hRegistration;

    HANDLE                          m_hDXVideoDecoderService;
    HANDLE                          m_hDXVideoProcessorService;

    IDirect3DDeviceManager9         *pD3DDeviceManager;

    IDirectXVideoDecoderService     *m_pDXVideoDecoderService;
    IDirectXVideoProcessorService   *m_pDXVideoProcessorService;

    IDirectXVideoDecoder            *m_pDXVideoDecoder;
    IDirectXVideoProcessor          *m_pDXRegistrationDevice;
    IDirect3DSurface9               *m_pD3DSurface;
    IDirect3DSurface9               *m_pDummySurface;
};

class AuxiliaryDevice
{
public:
    // default constructor
    AuxiliaryDevice(void);
    // default destructor
    ~AuxiliaryDevice(void);

    // initialize auxiliary device
    mfxStatus Initialize(IDirect3DDeviceManager9 *pD3DDeviceManager = NULL);

    // release auxiliary device object
    mfxStatus Release(void);

    // destroy acceleration service
    mfxStatus ReleaseAccelerationService();

    // check that specified device is supported
    mfxStatus IsAccelerationServiceExist(const GUID guid);

    // BeginFrame/Execute/EndFrame commands
    //HRESULT BeginFrame(IDirect3DSurface9 *pRenderTarget, void *pvPVPData);
    //HRESULT EndFrame(HANDLE *pHandleComplete);
    HRESULT Execute(mfxU32 func, void* input, mfxU32 inSize, void* output = NULL, mfxU32 outSize = 0);

    // query capabilities
    mfxStatus QueryAccelCaps(CONST GUID *pAccelGuid, void *pCaps, mfxU32 *puCapSize);

protected:

    mfxStatus CreateSurfaceRegistrationDevice(void);

    // current working service guid
    GUID m_Guid;

    // auxiliary D3D objects
    D3DAuxiliaryObjects D3DAuxObjects;
};

#endif // defined(_WIN32) || defined(_WIN64)