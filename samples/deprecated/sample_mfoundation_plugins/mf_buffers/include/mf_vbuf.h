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

/*********************************************************************************

File: mf_vbuf.h

Purpose: define MF Video Buffer class.

Defined Types:
  * MFVideoBuffer - [class] - video buffer class

Defined Global Functions:
  * MFCreateVideoBuffer - creates instance of MFVideoBuffer

Defined Global Variables:
  * m_gMemAlignDec - [mfxU16] - [dec out]: SYS mem align value
  * m_gMemAlignVpp - [mfxU16] - [vpp in]:  SYS mem align value
  * m_gMemAlignEnc - [mfxU16] - [enc in = vpp out]: SYS mem align value

*********************************************************************************/

#ifndef __MF_VBUF_H__
#define __MF_VBUF_H__

#include "mf_utils.h"

/*------------------------------------------------------------------------------*/

class IMfxFrameSurface : public IUnknown
{
public:
    virtual mfxFrameSurface1* GetMfxFrameSurface(void) = 0;
};

/*------------------------------------------------------------------------------*/

class MFMfxFrameSurface : public IMfxFrameSurface
{
public:
    MFMfxFrameSurface(void);
    virtual ~MFMfxFrameSurface(void);

    // IUnknown methods
    virtual STDMETHODIMP_(ULONG) AddRef(void);
    virtual STDMETHODIMP_(ULONG) Release(void);
    virtual STDMETHODIMP QueryInterface(REFIID iid, void** ppv);

    // IMfxFrameSurface methods
    virtual mfxFrameSurface1* GetMfxFrameSurface(void);
protected:
    long             m_nRefCount;
    mfxFrameSurface1 m_mfxSurface;
};

/*------------------------------------------------------------------------------*/

class MFVideoBuffer : public IMFMediaBuffer,
                      public IMF2DBuffer
{
public:
    MFVideoBuffer(void);
    virtual ~MFVideoBuffer(void);

    virtual HRESULT Alloc(mfxU16 w, mfxU16 h, mfxU16 p, mfxU32 fourcc);
    virtual HRESULT Pretend(mfxU16 w, mfxU16 h);
    virtual void PretendOn(bool flag) { m_bPretend = flag; };
    virtual void Free(void);

    // IUnknown methods
    virtual STDMETHODIMP_(ULONG) AddRef(void);
    virtual STDMETHODIMP_(ULONG) Release(void);
    virtual STDMETHODIMP QueryInterface(REFIID iid, void** ppv);

    // IMFMediaBuffer methods
    virtual STDMETHODIMP Lock(BYTE **ppbBuffer,
                              DWORD *pcbMaxLength,
                              DWORD *pcbCurrentLength);

    virtual STDMETHODIMP Unlock(void);

    virtual STDMETHODIMP GetCurrentLength(DWORD *pcbCurrentLength);

    virtual STDMETHODIMP SetCurrentLength(DWORD cbCurrentLength);

    virtual STDMETHODIMP GetMaxLength(DWORD *pcbMaxLength);

    // IMF2DBuffer methods
    virtual STDMETHODIMP Lock2D(BYTE **pbScanline0,
                                LONG *plPitch);

    virtual STDMETHODIMP Unlock2D(void);

    virtual STDMETHODIMP GetScanline0AndPitch(BYTE **pbScanline0,
                                              LONG *plPitch);

    virtual STDMETHODIMP IsContiguousFormat(BOOL *pfIsContiguous);

    virtual STDMETHODIMP GetContiguousLength(DWORD *pcbLength);

    virtual STDMETHODIMP ContiguousCopyTo(BYTE *pbDestBuffer,
                                          DWORD cbDestBuffer);

    virtual STDMETHODIMP ContiguousCopyFrom(const BYTE *pbSrcBuffer,
                                            DWORD cbSrcBuffer);

protected:
    // Reference count, critical section
    long   m_nRefCount;

    mfxU16 m_nWidth;
    mfxU16 m_nHeight;
    mfxU16 m_nPitch;
    mfxU32 m_FourCC;

    bool   m_bPretend;
    mfxU16 m_nPretendWidth;
    mfxU16 m_nPretendHeight;

    mfxU8* m_pData;
    mfxU8* m_pCData; // contiguous data
    mfxU32 m_nCDataAllocatedSize;
    mfxU32 m_nLength;
    mfxU32 m_nCLength;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFVideoBuffer(const MFVideoBuffer&);
    MFVideoBuffer& operator=(const MFVideoBuffer&);
};

/*------------------------------------------------------------------------------*/

extern mfxU16 m_gMemAlignDec;
extern mfxU16 m_gMemAlignVpp;
extern mfxU16 m_gMemAlignEnc;

/*------------------------------------------------------------------------------*/

extern HRESULT MFCreateVideoBuffer(mfxU16 w, mfxU16 h, mfxU16 p, mfxU32 fourcc,
                                   IMFMediaBuffer** ppBuffer);
extern HRESULT MFCreateMfxFrameSurface(IMfxFrameSurface** ppSurface);

#endif // #ifndef __MF_VBUF_H__
