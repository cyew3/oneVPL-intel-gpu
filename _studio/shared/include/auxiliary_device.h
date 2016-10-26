//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2011 Intel Corporation. All Rights Reserved.
//

#ifndef __AUXILIARY_DEVICE_H__
#define __AUXILIARY_DEVICE_H__

#include "encoding_ddi.h"

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
    mfxStatus Initialize(IDirect3DDeviceManager9        *pD3DDeviceManager = NULL,
                         IDirectXVideoDecoderService    *pVideoDecoderService = NULL);

    // release auxiliary device object
    mfxStatus Release(void);
    
    // create acceleration service
    mfxStatus CreateAccelerationService(const GUID guid, void *pCreateParams, UINT uCreateParamSize);

    // get current GUID
    GUID GetCurrentGuid() { return m_Guid; }

    // destroy acceleration service
    mfxStatus ReleaseAccelerationService();

    mfxStatus Register(IDirect3DSurface9 **pSources, mfxU32 iCount, BOOL bRegister);

    // registry surface through registration device
    mfxStatus RegistrySurface(IDirect3DSurface9 *source, IDirect3DSurface9 *destination);

    mfxStatus RegistrySurface(IDirect3DSurface9 *src);

    // create system memory surface
    mfxStatus CreateSurface(IDirect3DSurface9 **surface, mfxU32 width, mfxU32 height, D3DFORMAT format, D3DPOOL pool);

    // do fast copy
    mfxStatus FastCopyBlt(IDirect3DSurface9 *source, IDirect3DSurface9 *destination);

    // check that specified device is supported
    mfxStatus IsAccelerationServiceExist(const GUID guid);

    // BeginFrame/Execute/EndFrame commands
    HRESULT BeginFrame(IDirect3DSurface9 *pRenderTarget, void *pvPVPData);
    HRESULT EndFrame(HANDLE *pHandleComplete);
    HRESULT Execute(mfxU32 func, void* input, mfxU32 inSize, void* output = NULL, mfxU32 outSize = 0);

    // query supported formats
    mfxStatus QueryAccelFormats(CONST GUID *pAccelGuid, D3DFORMAT **ppFormats, UINT *puCount);

    // query capabilities
    mfxStatus QueryAccelCaps(CONST GUID *pAccelGuid, void *pCaps, UINT *puCapSize);

    // query supported render target formats
    mfxStatus QueryAccelRTFormats(CONST GUID *pAccelGuid, D3DFORMAT **ppFormats, UINT *puCount);

protected:
    // create video processing device
    //mfxStatus CreateVideoProcessingDevice();

    mfxStatus CreateSurfaceRegistrationDevice(void);

    // current working service guid
    GUID m_Guid;

    // auxiliary D3D objects
    D3DAuxiliaryObjects D3DAuxObjects;
};

#endif // __AUXILIARY_DEVICE_H__