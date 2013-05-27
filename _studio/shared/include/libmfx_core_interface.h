/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

File Name: libmfx_core_interface.h

\* ****************************************************************************** */

#ifndef __LIBMFX_CORE_INTERFACE_H__
#define __LIBMFX_CORE_INTERFACE_H__

#include "mfx_common.h"
#include <mfxvideo++int.h>


// {1F5BB140-6BB4-416e-81FF-4A8C030FBDC6}
static const 
MFX_GUID  MFXIVideoCORE_GUID = 
{ 0x1f5bb140, 0x6bb4, 0x416e, { 0x81, 0xff, 0x4a, 0x8c, 0x3, 0xf, 0xbd, 0xc6 } };

// {3F3C724E-E7DC-469a-A062-B6A23102F7D2}
static const 
MFX_GUID  MFXICORED3D_GUID = 
{ 0x3f3c724e, 0xe7dc, 0x469a, { 0xa0, 0x62, 0xb6, 0xa2, 0x31, 0x2, 0xf7, 0xd2 } };

// {C9613F63-3EA3-4D8C-8B5C-96AF6DC2DB0F}
static const MFX_GUID  MFXICORED3D11_GUID = 
{ 0xc9613f63, 0x3ea3, 0x4d8c, { 0x8b, 0x5c, 0x96, 0xaf, 0x6d, 0xc2, 0xdb, 0xf } };

// {B0FCB183-1A6D-4f00-8BAF-93F285ACEC93}
static const MFX_GUID MFXICOREVAAPI_GUID = 
{ 0xb0fcb183, 0x1a6d, 0x4f00, { 0x8b, 0xaf, 0x93, 0xf2, 0x85, 0xac, 0xec, 0x93 } };


// Try to obtain required interface
// Declare a template to query an interface
template <class T> inline
T *QueryCoreInterface(VideoCORE* pUnk, const MFX_GUID &guid = T::getGuid())
{
    void *pInterface = NULL;

    // query the interface
    if (pUnk)
    {
        pInterface = pUnk->QueryCoreInterface(guid);
        // cast pointer returned to the required core interface
        return reinterpret_cast<T *>(pInterface);
    }
    else
    {
        return NULL;
    }

} // T *QueryCoreInterface(MFXIUnknown *pUnk, const MFX_GUID &guid)

#if defined (MFX_VA_WIN)

struct IDirectXVideoDecoderService;
struct IDirect3DDeviceManager9;

// to obtaion D3D services from the Core
struct D3D9Interface
{
    static const MFX_GUID & getGuid()
    {
        return MFXICORED3D_GUID;
    }

    virtual mfxStatus              GetD3DService(mfxU16 width,
                                                 mfxU16 height,
                                                 IDirectXVideoDecoderService **ppVideoService = NULL,
                                                 bool isTemporal = false) = 0;
    
    virtual IDirect3DDeviceManager9 * GetD3D9DeviceManager(void) = 0;
};

struct ID3D11Device;
struct ID3D11VideoDevice;
struct ID3D11DeviceContext;
struct ID3D11VideoContext;


struct D3D11Interface
{
    static const MFX_GUID getGuid()
    {
        return MFXICORED3D11_GUID;
    }

    virtual ID3D11Device * GetD3D11Device(bool isTemporal = false) = 0;
    virtual ID3D11VideoDevice * GetD3D11VideoDevice(bool isTemporal = false) = 0;

    virtual ID3D11DeviceContext * GetD3D11DeviceContext(bool isTemporal = false) = 0;
    virtual ID3D11VideoContext * GetD3D11VideoContext(bool isTemporal = false) = 0;
};

#elif defined (MFX_VA_LINUX)
    struct VAAPIInterface
    {
        static const MFX_GUID & getGuid()
        {
            return MFXICOREVAAPI_GUID;
        }
    
    //    virtual mfxStatus              GetD3DService(mfxU16 width,
    //                                                 mfxU16 height,
    //                                                 VADisplay** pDisplay = NULL) = 0;      
    };
#endif

#endif // __LIBMFX_CORE_INTERFACE_H__
/* EOF */
