// Copyright (c) 2009-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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