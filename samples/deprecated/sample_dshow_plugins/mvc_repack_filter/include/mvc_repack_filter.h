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

#pragma once

#include "avc_frame_splitter.h"

#define MAX_INPUT_PIN_QTY 2
#define BITSTREAM_SET_QTY 10
#define MAX_FRAME_SIZE    2*1024*1024

class CRepackInputPin;
class CRepackOutputPin;

class  CMVCRepackFilter : public CBaseFilter
{
public:

    DECLARE_IUNKNOWN;

    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    CMVCRepackFilter(TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr);
    ~CMVCRepackFilter();

    STDMETHODIMP            Pause();

    // these methods should be overridden
    virtual CBasePin*       GetPin(int nIndex);
    virtual int             GetPinCount(void);

    HRESULT                 DeliverEndOfStream(void);

    std::list<mfxBitstream> set1;
    std::list<mfxBitstream> set2;

    std::list<mfxBitstream> setTotal;

    int                     m_nViewToWrite;

    HRESULT                 PutDataToWriter(AVCFrameSplitterInfo* frame, mfxU8 viewnum);
    HRESULT                 DeliverBitstream(mfxBitstream* bs);

protected:

    bool                    CreateInputPin(void);
    bool                    CreateOutputPin(void);

    void                    CreatePins(void);
    void                    RemovePins(void);

    int                     m_nInputPinsCount;

    CRepackOutputPin*       m_pOutputPin;
    CRepackInputPin*        m_pInput[MAX_INPUT_PIN_QTY];

    mfxU8                   m_nPinsActive;

    CCritSec                m_csShared;    // Protects shared data.
};

class CRepackInputPin : public CBaseInputPin
{
public:
    DECLARE_IUNKNOWN;

    CRepackInputPin(TCHAR *pName, CBaseFilter  *pFilter, HRESULT *phr, CCritSec *cs, LPCWSTR pPinName, INT PinNumber);
    ~CRepackInputPin();

    HRESULT         GetMediaType(int iPosition, CMediaType *pMediaType);
    HRESULT         CheckMediaType(const CMediaType *pmt);

    STDMETHODIMP    Receive(IMediaSample *pSample);
    STDMETHODIMP    EndOfStream();

protected:

    mfxU8                   m_nPinNumber;

    AVCFrameSplitter*       m_pNALSplitter;
    AVCFrameSplitterInfo*   m_pFrame;

    mfxBitstream            m_bsInput;
};

class CRepackOutputPin : public CBaseOutputPin
{
public:
    DECLARE_IUNKNOWN;

    CRepackOutputPin(TCHAR *pName, CBaseFilter *pFilter, HRESULT *phr, CCritSec *cs, LPCWSTR pPinName);

    HRESULT         CheckMediaType(const CMediaType *pmt) { return S_OK; };
    HRESULT         DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * ppropInputRequest);
    HRESULT         GetMediaType(int iPosition,CMediaType *pMediaType);

    // Override since the life time of pins and filters are not the same
    STDMETHODIMP_(ULONG)    NonDelegatingAddRef();
    STDMETHODIMP_(ULONG)    NonDelegatingRelease();
};
