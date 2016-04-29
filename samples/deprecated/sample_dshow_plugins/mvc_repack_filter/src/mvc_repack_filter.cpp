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

#include "atlbase.h"
#include <streams.h>

#include "mvc_repack_filter.h"
#include "mfx_filter_guid.h"
#include "filter_defs.h"
#include "mfx_filter_register.h"

//{8D2D71CB-243F-45E3-B2D8-5FD7967EC09B}
static GUID MEDIASUBTYPE_H264_CUST= {0x8D2D71CB, 0x243F, 0x45E3, {0xB2, 0xD8, 0x5F, 0xD7, 0x96, 0x7E, 0xC0, 0x9B}};

CUnknown* WINAPI CMVCRepackFilter::CreateInstance(LPUNKNOWN punk, HRESULT* phr)
{
    CMVCRepackFilter *pNewObject = new CMVCRepackFilter(FILTER_NAME, punk, phr);

    if (NULL == pNewObject)
    {
        *phr = E_OUTOFMEMORY;
    }

    return pNewObject;
}

CMVCRepackFilter::CMVCRepackFilter(TCHAR *tszName,LPUNKNOWN punk, HRESULT *phr) :
CBaseFilter(tszName, punk, &m_csShared, FILTER_GUID)
{
    CreatePins();
    m_nViewToWrite = 0;

    for (mfxU8 i = 0; i < BITSTREAM_SET_QTY; i++)
    {
        mfxBitstream bs;
        memset(&bs, 0, sizeof(mfxBitstream));

        bs.DataLength = MAX_FRAME_SIZE;
        bs.Data = new mfxU8[bs.DataLength];

        memset(bs.Data, 0, bs.DataLength);

        setTotal.push_back(bs);
    }

    m_nPinsActive = MAX_INPUT_PIN_QTY;
}

CMVCRepackFilter::~CMVCRepackFilter()
{
    CAutoLock cObjectLock(m_pLock);
    RemovePins();

    std::list<mfxBitstream>::iterator it;
    for (it = set1.begin(); it != set1.end(); it++)
    {
        MSDK_SAFE_DELETE_ARRAY(it->Data);
    }
    for (it = set2.begin(); it != set2.end(); it++)
    {
        MSDK_SAFE_DELETE_ARRAY(it->Data);
    }
    for (it = setTotal.begin(); it != setTotal.end(); it++)
    {
        MSDK_SAFE_DELETE_ARRAY(it->Data);
    }
}


STDMETHODIMP CMVCRepackFilter::Pause()
{
    for(mfxU8 i = 0; i < MAX_INPUT_PIN_QTY; i++)
    {
        if (!m_pInput[i]->IsConnected())
        {
            return E_FAIL;
        }
    }

    if (!m_pOutputPin->IsConnected())
    {
        return E_FAIL;
    };

    return CBaseFilter::Pause();
}

int CMVCRepackFilter::GetPinCount()
{
    CAutoLock cObjectLock(m_pLock);
    return m_nInputPinsCount + 1;
}

void CMVCRepackFilter::RemovePins(void)
{
    m_csShared.Lock();

    for(mfxU8 i = 0; i < MAX_INPUT_PIN_QTY; i++)
    {
        if(NULL != m_pInput[i])
        {
            if(!(m_pInput[i]->IsConnected()))
            {
                MSDK_SAFE_DELETE(m_pInput[i]);
                m_nInputPinsCount--;
            }
        }
    }

    MSDK_SAFE_DELETE(m_pOutputPin);

    m_csShared.Unlock();

    IncrementPinVersion();

    return;

};

void CMVCRepackFilter::CreatePins(void)
{
    m_nInputPinsCount = 0;

    for(mfxU8 i = 0; i < MAX_INPUT_PIN_QTY; i++)
    {
        m_pInput[i] = NULL;
    }

    m_pOutputPin = NULL;

    //base view pin
    CreateInputPin();
    //second view pin
    CreateInputPin();
    //create output pin
    CreateOutputPin();
};

bool CMVCRepackFilter::CreateInputPin()
{
    HRESULT hr = S_OK;
    TCHAR   strPinName[15];

    if (m_nInputPinsCount >= MAX_INPUT_PIN_QTY)
    {
        return false;
    }

    if (0 == m_nInputPinsCount)
    {
        _stprintf_s(strPinName, 15, _T("Base view %d"), m_nInputPinsCount);
    }
    else
    {
        _stprintf_s(strPinName, 15, _T("Second view %d"), m_nInputPinsCount);
    }

    m_pInput[m_nInputPinsCount] = new CRepackInputPin(strPinName, this, &hr, &m_csShared, strPinName, m_nInputPinsCount);

    if (m_pInput[m_nInputPinsCount])
    {
        m_csShared.Lock();
        m_nInputPinsCount++;
        m_csShared.Unlock();

        return true;
    }

    return false;
}

bool CMVCRepackFilter::CreateOutputPin()
{
    HRESULT hr = S_OK;
    m_pOutputPin = new CRepackOutputPin(NAME("MVC Stream"), this, &hr, &m_csShared, L"MVC Stream");

    return m_pOutputPin ? true : false;
}

CBasePin* CMVCRepackFilter::GetPin(int nIndex)
{
    if (nIndex < MAX_INPUT_PIN_QTY && NULL != m_pInput[nIndex])
    {
        return m_pInput[nIndex];
    }

    if (m_pOutputPin && nIndex < m_nInputPinsCount + 1)
    {
        return m_pOutputPin;
    }

    return NULL;
}

HRESULT CMVCRepackFilter::DeliverEndOfStream()
{
    HRESULT hr = S_OK;
    CAutoLock cObjectLock(m_pLock);

    m_nPinsActive--;

    if (0 == m_nPinsActive)
    {
        do
        {
            hr = PutDataToWriter(NULL, 0);
        } while (S_OK == hr);

        m_pOutputPin->DeliverEndOfStream();

        m_nPinsActive = MAX_INPUT_PIN_QTY;
    }

    return S_OK;
}

HRESULT CMVCRepackFilter::DeliverBitstream(mfxBitstream* bs)
{
    mfxU8* pBuffer = NULL;
    mfxU32 nLength = bs->DataLength;
    CComPtr<IMediaSample> pOutSample;

    HRESULT hr = m_pOutputPin->GetDeliveryBuffer(&pOutSample, NULL, NULL, 0);

    if (S_OK == hr)
    {
        hr = pOutSample->GetPointer(&pBuffer);
    }

    //check that buffer size is enough
    if (S_OK == hr && (mfxU32)pOutSample->GetSize() < nLength)
    {
        hr = E_OUTOFMEMORY;
    }

    if (S_OK == hr)
    {
        MSDK_MEMCPY(pBuffer, bs->Data, nLength);
        pOutSample->SetActualDataLength(nLength);
    }

    if (S_OK == hr)
    {
        hr = m_pOutputPin->Deliver(pOutSample);
    }

    return hr;
};

HRESULT CMVCRepackFilter::PutDataToWriter(AVCFrameSplitterInfo* frame, mfxU8 viewnum)
{
    HRESULT hr = E_FAIL;

    //to get data from pins in some equal proportions
    std::list<mfxBitstream>* pSet = (viewnum == 0) ? &set1 : &set2;
    while ((pSet->size() >= BITSTREAM_SET_QTY / 2.0) && (State_Running == m_State))
    {
        if (m_nPinsActive < MAX_INPUT_PIN_QTY)
        {
            break;
        }
        Sleep(10);
    }

    CAutoLock cObjectLock(m_pLock);

    if (frame && setTotal.size())
    {
        mfxBitstream bitstream = setTotal.front();
        setTotal.pop_front();

        MSDK_MEMCPY(bitstream.Data, frame->Data, frame->DataLength);
        bitstream.DataLength = frame->DataLength;

        if (0 == viewnum)
        {
            set1.push_back(bitstream);
        }
        else
        {
            set2.push_back(bitstream);
        }
    }

    if (0 == m_nViewToWrite && set1.size())
    {
        hr = DeliverBitstream(&set1.front());

        set1.front().DataLength = 0;
        setTotal.push_back(set1.front());

        set1.pop_front();

        m_nViewToWrite = 1;
    }

    if (1 == m_nViewToWrite && set2.size())
    {
        hr = DeliverBitstream(&set2.front());

        set2.front().DataLength = 0;
        setTotal.push_back(set2.front());

        set2.pop_front();

        m_nViewToWrite = 0;
    }

    return hr;
};

//CRepackInputPin
CRepackInputPin::CRepackInputPin( TCHAR *pName, CBaseFilter *pFilter, HRESULT *phr, CCritSec *cs, LPCWSTR pPinName, INT PinNumber) :
CBaseInputPin(pName, pFilter, cs, phr, pPinName)
{
    m_pFilter = pFilter;
    m_nPinNumber = (mfxU8)PinNumber;

    memset(&m_bsInput, 0, sizeof(m_bsInput));

    //hardcoded size expected to be always enough to store bytes from up-stream filter in case of sample
    m_bsInput.Data = new mfxU8[MAX_FRAME_SIZE / 2];

    m_pNALSplitter = new AVCFrameSplitter();
    m_pFrame = new AVCFrameSplitterInfo;
}

CRepackInputPin::~CRepackInputPin()
{
    MSDK_SAFE_DELETE(m_pNALSplitter);
    MSDK_SAFE_DELETE(m_pFrame);
    MSDK_SAFE_DELETE(m_bsInput.Data);
};

STDMETHODIMP CRepackInputPin::Receive(IMediaSample *pSample)
{
    HRESULT         hr = S_OK;

    mfxStatus       sts = MFX_ERR_NONE;
    mfxBitstream    inBs;
    BYTE*           pBuffer = NULL;

    memset(&inBs, 0, sizeof(inBs));

    hr = pSample->GetPointer(&pBuffer);

    if (S_OK == hr)
    {
        if (0 == m_bsInput.DataLength)
        {
            inBs.Data = pBuffer;
            inBs.DataLength = pSample->GetActualDataLength();
        }
        else
        {
            inBs.DataLength = pSample->GetActualDataLength() + m_bsInput.DataLength;
            inBs.Data = m_bsInput.Data;

            MSDK_MEMCPY(inBs.Data + m_bsInput.DataLength, pBuffer, pSample->GetActualDataLength());

            m_bsInput.DataLength =0;
        }

        sts = m_pNALSplitter->GetFrame(&inBs, &m_pFrame);

        if (inBs.DataLength)
        {
            m_bsInput.DataLength = inBs.DataLength;
            MSDK_MEMCPY(m_bsInput.Data, inBs.Data + inBs.DataOffset, inBs.DataLength);
        };

        if (MFX_ERR_NONE == sts)
        {
            ((CMVCRepackFilter*)m_pFilter)->PutDataToWriter(m_pFrame, m_nPinNumber);
            m_pNALSplitter->ResetCurrentState();
        }
    }

    return CBaseInputPin::Receive(pSample);
}

HRESULT CRepackInputPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
    if (0 == iPosition || 1 == iPosition)
    {
        pMediaType->majortype = MEDIATYPE_Video;
        pMediaType->subtype   = MEDIASUBTYPE_H264;
        return S_OK;
    }

    return VFW_S_NO_MORE_ITEMS;
}

HRESULT CRepackInputPin::CheckMediaType(const CMediaType *pmt)
{
    if (MEDIATYPE_Video == pmt->majortype && FORMAT_MPEG2_VIDEO == pmt->formattype)
    {
        if (MEDIASUBTYPE_H264 == *pmt->Subtype() || MEDIASUBTYPE_H264_CUST == *pmt->Subtype())
        {
            if (pmt->cbFormat > sizeof(VIDEOINFOHEADER2))
            {
                MSDK_MEMCPY(m_bsInput.Data, pmt->pbFormat + sizeof(VIDEOINFOHEADER2), pmt->cbFormat - sizeof(VIDEOINFOHEADER2));
                m_bsInput.DataLength = pmt->cbFormat - sizeof(VIDEOINFOHEADER2);
            }

            return S_OK;
        }
    }

    return VFW_E_TYPE_NOT_ACCEPTED;
};

HRESULT CRepackInputPin::EndOfStream(void)
{
    ((CMVCRepackFilter*)m_pFilter)->DeliverEndOfStream();
    return S_OK;
};

CRepackOutputPin::CRepackOutputPin(TCHAR *pName, CBaseFilter  *pFilter, HRESULT *phr, CCritSec *cs, LPCWSTR pPinName) :
CBaseOutputPin(pName, pFilter, cs, phr, pPinName)
{
    m_pFilter = pFilter;
}

STDMETHODIMP_(ULONG) CRepackOutputPin::NonDelegatingAddRef()
{
    CAutoLock cObjectLock(m_pLock);
    return (++m_cRef);
};

STDMETHODIMP_(ULONG) CRepackOutputPin::NonDelegatingRelease()
{
    CAutoLock cObjectLock(m_pLock);
    return (--m_cRef) ? m_cRef : 0;
};

HRESULT CRepackOutputPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
    if (0 == iPosition)
    {
        pMediaType->majortype = MEDIATYPE_Video;
        return S_OK;
    }

    return VFW_S_NO_MORE_ITEMS;
}

HRESULT CRepackOutputPin::DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pProperties)
{
    // faked allocator
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = MAX_FRAME_SIZE;

    ALLOCATOR_PROPERTIES Actual;
    HRESULT hr = pAlloc->SetProperties(pProperties,&Actual);

    if (FAILED(hr))
    {
        return hr;
    }

    // is this allocator unsuitable
    if (Actual.cbBuffer < pProperties->cbBuffer)
    {
        return E_FAIL;
    }

    return S_OK;
}
