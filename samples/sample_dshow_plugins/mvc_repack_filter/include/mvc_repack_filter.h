/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2011 Intel Corporation. All Rights Reserved.
//
//
*/

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
