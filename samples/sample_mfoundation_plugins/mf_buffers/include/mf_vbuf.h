/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

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
