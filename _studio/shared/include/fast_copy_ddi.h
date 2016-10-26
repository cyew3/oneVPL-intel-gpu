//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009 Intel Corporation. All Rights Reserved.
//

#ifndef __FAST_COPY_DDI_H__
#define __FAST_COPY_DDI_H__

#include "fast_copy.h"
#include "auxiliary_device.h"

class FastCopyDDI: public FastCopy
{
public:
    
    // constructor   
    FastCopyDDI(IDirect3DDeviceManager9 *pD3DDeviceManager);

    // destructor
    ~FastCopyDDI(void);

    // initialize available functionality
    mfxStatus Initialize(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameAllocator *allocator);

    // release object
    mfxStatus Release(void);

    // copy memory by streaming
    mfxStatus Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi);

    // copy memory by ddi
    mfxStatus Copy(IDirect3DSurface9 *pDst, IDirect3DSurface9 *pSrc);

protected:

    // synchronize threads
    mfxStatus Synchronize(void);

    // pointer on device manager object
    IDirect3DDeviceManager9 *pD3DDeviceManager;

    // ddi object
    AuxiliaryDevice *m_pDevice;

    // surfaces
    IDirect3DSurface9 *m_pSystemMemorySurface;

};

#endif // __FAST_COPY_DDI_H__