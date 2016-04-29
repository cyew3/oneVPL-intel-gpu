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

File: mf_vbuf.cpp

Purpose: define MF Video Buffer class.

Notes:
  - "Pretend" feature is workaround to avoid input type changing on MSFT encoder.

*********************************************************************************/
#include "mf_vbuf.h"

/*------------------------------------------------------------------------------*/

mfxU16 m_gMemAlignDec = 128; // [dec/vpp out]: SYS mem align value
mfxU16 m_gMemAlignVpp = 1;   // [enc/vpp in]:  SYS mem align value
mfxU16 m_gMemAlignEnc = 1;   // [enc in = vpp out]: SYS mem align value

/*------------------------------------------------------------------------------*/

HRESULT MFCreateVideoBuffer(mfxU16 w, mfxU16 h, mfxU16 p, mfxU32 fourcc,
                            IMFMediaBuffer** ppBuffer)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    MFVideoBuffer* pBuf = NULL;

    CHECK_POINTER(ppBuffer, E_POINTER);

    SAFE_NEW(pBuf, MFVideoBuffer);
    CHECK_EXPRESSION(pBuf, E_OUTOFMEMORY);
    hr = pBuf->Alloc(w, h, p, fourcc);
    if (SUCCEEDED(hr)) hr = pBuf->QueryInterface(IID_IMFMediaBuffer, (void**)ppBuffer);
    if (FAILED(hr))
    {
        SAFE_DELETE(pBuf);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFCreateMfxFrameSurface(IMfxFrameSurface** ppSurface)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    MFMfxFrameSurface* pSrf = NULL;

    CHECK_POINTER(ppSurface, E_POINTER);

    SAFE_NEW(pSrf, MFMfxFrameSurface);
    CHECK_EXPRESSION(pSrf, E_OUTOFMEMORY);
    if (SUCCEEDED(hr)) hr = pSrf->QueryInterface(IID_IMfxFrameSurface, (void**)ppSurface);
    if (FAILED(hr))
    {
        SAFE_DELETE(pSrf);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/
//MFMfxFrameSurface

MFMfxFrameSurface::MFMfxFrameSurface(void):
    m_nRefCount(0)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    memset(&m_mfxSurface, 0, sizeof(mfxFrameSurface1));
    DllAddRef();
}

/*------------------------------------------------------------------------------*/

MFMfxFrameSurface::~MFMfxFrameSurface(void)
{
    DllRelease();
}

/*------------------------------------------------------------------------------*/
// IUnknown methods

ULONG MFMfxFrameSurface::AddRef(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_I(MF_TL_GENERAL, m_nRefCount+1);
    return InterlockedIncrement(&m_nRefCount);
}

ULONG MFMfxFrameSurface::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    MFX_LTRACE_I(MF_TL_GENERAL, uCount);
    if (uCount == 0) delete this;
    // For thread safety, return a temporary variable.
    return uCount;
}

HRESULT MFMfxFrameSurface::QueryInterface(REFIID iid, void** ppv)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (!ppv) return E_POINTER;
    if ((iid == IID_IUnknown) || (iid == IID_IMfxFrameSurface))
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IUnknown or IMfxFrameSurface");
        *ppv = static_cast<IMfxFrameSurface*>(this);
    }
    else
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, iid);
        return E_NOINTERFACE;
    }
    AddRef();
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

mfxFrameSurface1* MFMfxFrameSurface::GetMfxFrameSurface(void)
{
    return &m_mfxSurface;
}

/*------------------------------------------------------------------------------*/
// MFVideoBuffer

MFVideoBuffer::MFVideoBuffer(void):
    m_nRefCount(0),
    m_pData(NULL),
    m_pCData(NULL),
    m_nCDataAllocatedSize(0),
    m_nWidth(0),
    m_nHeight(0),
    m_nPitch(0),
    m_FourCC(0),
    m_nLength(0),
    m_nCLength(0),
    m_bPretend(false),
    m_nPretendWidth(0),
    m_nPretendHeight(0)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
}

/*------------------------------------------------------------------------------*/

MFVideoBuffer::~MFVideoBuffer(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    Free();
}

/*------------------------------------------------------------------------------*/

HRESULT MFVideoBuffer::Alloc(mfxU16 w, mfxU16 h, mfxU16 p, mfxU32 fourcc)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxU32 length = 3*p*h/2;
    CHECK_EXPRESSION(w && h && p && (w <= p), E_FAIL);
    CHECK_EXPRESSION((MFX_FOURCC_NV12 == fourcc), E_FAIL);
    CHECK_EXPRESSION(!m_pData, E_FAIL);

    if (!m_pData) m_pData = (mfxU8*)malloc(length);
    CHECK_EXPRESSION(m_pData, E_OUTOFMEMORY);
    m_FourCC   = fourcc;
    m_nWidth   = w;
    m_nHeight  = h;
    m_nPitch   = p;
    m_nLength  = length;
    m_nCLength = 3*w*h/2;
    m_nPretendWidth  = w;
    m_nPretendHeight = h;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFVideoBuffer::Pretend(mfxU16 w, mfxU16 h)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    CHECK_EXPRESSION(h && w, E_FAIL);

    m_nPretendWidth  = w;
    m_nPretendHeight = h;
    m_nCLength       = 3*w*h/2;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

void MFVideoBuffer::Free(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    SAFE_FREE(m_pCData);
    m_nCDataAllocatedSize = 0;
    SAFE_FREE(m_pData);
}

/*------------------------------------------------------------------------------*/
// IUnknown methods

ULONG MFVideoBuffer::AddRef(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_I(MF_TL_GENERAL, m_nRefCount+1);
    return InterlockedIncrement(&m_nRefCount);
}

ULONG MFVideoBuffer::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    MFX_LTRACE_I(MF_TL_GENERAL, uCount);
    if (uCount == 0) delete this;
    // For thread safety, return a temporary variable.
    return uCount;
}

HRESULT MFVideoBuffer::QueryInterface(REFIID iid, void** ppv)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (!ppv) return E_POINTER;
    if ((iid == IID_IUnknown) || (iid == IID_IMFMediaBuffer))
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IUnknown or IMFMediaBuffer");
        *ppv = static_cast<IMFMediaBuffer*>(this);
    }
    else if (iid == IID_IMF2DBuffer)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IMF2DBuffer");
        *ppv = static_cast<IMF2DBuffer*>(this);
    }
    else if (iid == IID_MFVideoBuffer)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "MFVideoBuffer");
        *ppv = static_cast<MFVideoBuffer*>(this);
    }
    else
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, iid);
        return E_NOINTERFACE;
    }
    AddRef();
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

// IMFMediaBuffer methods
STDMETHODIMP MFVideoBuffer::Lock(BYTE** ppbBuffer,
                                 DWORD* pcbMaxLength,
                                 DWORD* pcbCurrentLength)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(m_pData, E_FAIL);
    CHECK_POINTER(ppbBuffer, E_POINTER);
//    CHECK_POINTER(pcbMaxLength, E_POINTER); // can be NULL by MSFT spec
//    CHECK_POINTER(pcbCurrentLength, E_POINTER); // can be NULL by MSFT spec

    if ((m_nPitch > m_nWidth) || m_bPretend)
    {
        mfxU32 h = (m_bPretend)? m_nPretendHeight: m_nHeight;
        mfxU32 w = (m_bPretend)? m_nPretendWidth: m_nWidth;
        mfxU32 length = 3*w*h/2;

        if (!m_pCData || (length != m_nCLength))
        {
            SAFE_FREE(m_pCData);
            m_pCData = (mfxU8*)calloc(length, sizeof(mfxU8));
            m_nCDataAllocatedSize = length;
        }
        if (!m_pCData)
        {
            m_nCDataAllocatedSize = 0;
            hr = E_OUTOFMEMORY;
            ATLASSERT(NULL != m_pCData);
        }
        else
        {
            mfxU32 i = 0;
            mfxU8 *buf_src = NULL, *buf_dst = NULL;
            mfxU32 copy_h = (m_nHeight < m_nPretendHeight)? m_nHeight: m_nPretendHeight;
            mfxU32 copy_w = (m_nWidth < m_nPretendWidth)? m_nWidth: m_nPretendWidth;

            m_nCLength = length;
            for (i = 0; i < copy_h; ++i)
            {
                memcpy_s(m_pCData + i*w, (m_nCDataAllocatedSize-i*w)*sizeof(mfxU8), m_pData + i*m_nPitch, copy_w);
            }
            buf_src = m_pData + m_nPitch*m_nHeight;
            buf_dst = m_pCData + w*h;
            for (i = 0; i < (mfxU32)copy_h/2; ++i)
            {
                memcpy_s(buf_dst + i*w, (m_nCDataAllocatedSize-w*h-i*w)*sizeof(mfxU8), buf_src + i*m_nPitch, copy_w);
            }
            *ppbBuffer = m_pCData;
        }
    }
    else *ppbBuffer = m_pData;
    if (pcbMaxLength) *pcbMaxLength = m_nCLength;
    if (pcbCurrentLength) *pcbCurrentLength = m_nCLength;
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::Unlock(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    return S_OK;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::GetCurrentLength(DWORD* pcbCurrentLength)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxU32 w = (m_bPretend)? m_nPretendWidth: m_nWidth;
    mfxU32 h = (m_bPretend)? m_nPretendHeight: m_nHeight;

    CHECK_EXPRESSION(m_pData, E_FAIL);
    CHECK_POINTER(pcbCurrentLength, E_POINTER);
    *pcbCurrentLength = 3*w*h/2;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::SetCurrentLength(DWORD /*cbCurrentLength*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    return S_OK;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::GetMaxLength(DWORD* pcbMaxLength)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    CHECK_EXPRESSION(m_pData, E_FAIL);
    CHECK_POINTER(pcbMaxLength, E_POINTER);
    *pcbMaxLength = m_nCLength;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

// IMF2DBuffer methods
STDMETHODIMP MFVideoBuffer::Lock2D(BYTE **pbScanline0,
                                   LONG *plPitch)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(m_pData, E_FAIL);
    CHECK_POINTER(pbScanline0, E_POINTER);
    CHECK_POINTER(plPitch, E_POINTER);

    if (!m_bPretend)
    {
        *pbScanline0 = m_pData;
        *plPitch = m_nPitch;
    }
    else
    {
        DWORD max_len = 0, cur_len = 0;

        hr = Lock(pbScanline0, &max_len, &cur_len);
        *plPitch = m_nPretendWidth;
    }
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::Unlock2D(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    return S_OK;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::GetScanline0AndPitch(BYTE **pbScanline0,
                                                 LONG *plPitch)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(m_pData, E_FAIL);
    CHECK_POINTER(pbScanline0, E_POINTER);
    CHECK_POINTER(plPitch, E_POINTER);

    if (!m_bPretend)
    {
        *pbScanline0 = m_pData;
        *plPitch = m_nPitch;
    }
    else
    {
        DWORD max_len = 0, cur_len = 0;

        hr = Lock(pbScanline0, &max_len, &cur_len);
        *plPitch = m_nPretendWidth;
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::IsContiguousFormat(BOOL *pfIsContiguous)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    CHECK_EXPRESSION(m_pData, E_FAIL);
    CHECK_POINTER(pfIsContiguous, E_POINTER);
    if ((m_nPitch > m_nWidth) || m_bPretend) *pfIsContiguous = FALSE;
    else *pfIsContiguous = TRUE;

    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::GetContiguousLength(DWORD *pcbLength)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxU32 w = (m_bPretend)? m_nPretendWidth: m_nWidth;
    mfxU32 h = (m_bPretend)? m_nPretendHeight: m_nHeight;

    CHECK_EXPRESSION(m_pData, E_FAIL);
    CHECK_POINTER(pcbLength, E_POINTER);
    *pcbLength = 3*w*h/2;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::ContiguousCopyTo(BYTE* /*pbDestBuffer*/,
                                             DWORD /*cbDestBuffer*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

STDMETHODIMP MFVideoBuffer::ContiguousCopyFrom(const BYTE* /*pbSrcBuffer*/,
                                               DWORD /*cbSrcBuffer*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    return E_NOTIMPL;
}
