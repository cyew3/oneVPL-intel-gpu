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

#include <afx.h>
#include "mfx_filter_guid.h"
#include "mfx_video_enc_filter_utils.h"

#include "base_encoder.h"
#include "sample_defs.h"

#include "mfx_video_enc_proppage.h"
#include "wmsdkidl.h"

#include "mfxvideo.h"
#include "mfxvideo++.h"

#include "bs_adapter.h"
#include "mfx_filter_externals.h"

#define FILTER_MERIT MERIT_DO_NOT_USE

class CEncoderOutputPin : public CTransformOutputPin
{
public:
    CEncoderOutputPin(CTransformFilter* pTransformFilter, HRESULT* phr):
    CTransformOutputPin(NAME("OutputPin"), pTransformFilter, phr, L"Out"){};

    DECLARE_IUNKNOWN;
};

class CEncVideoFilter : public CTransformFilter, public ISpecifyPropertyPages, public IAboutProperty,
                        public IConfigureVideoEncoder, public IVideoEncoderProperty, INotifier,
                        public IExposeMfxMemTypeFlags, public IShareMfxSession
{
public:
    DECLARE_IUNKNOWN;

    // Default constructor
    CEncVideoFilter(TCHAR* tszName, LPUNKNOWN punk, const GUID VIDEOCodecGUID, HRESULT* phr,
        mfxU16 APIVerMinor = 1, mfxU16 APIVerMajor = 1);
    // Destructor
    ~CEncVideoFilter(void);

    // Overridden pure virtual functions from CTransformFilter base class

    // Notify that no more data passed through graph
    HRESULT             EndOfStream(void);
    // Receive new media portion
    HRESULT             Receive(IMediaSample* pSample);
    // Get new frame
    HRESULT             GetFrame(void);
    // Start data streaming through graph
    HRESULT             StartStreaming(void);
    // Stop data streaming
    HRESULT             StopStreaming(void);
    // Check compatibility of connected pin(s)
    HRESULT             CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut) {return S_OK;};
    // Decide buffer size
    HRESULT             DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties);
    // Get connection media type
    HRESULT             GetMediaType(int iPosition, CMediaType* pMediaType);
    // These must be supplied in a derived class
    HRESULT             CheckInputType(const CMediaType* mtIn);
    // break connection
    HRESULT             BreakConnect(PIN_DIRECTION dir);
    // override this to say what interfaces we support where
    STDMETHODIMP        NonDelegatingQueryInterface(REFIID riid, void** ppv);

    HRESULT             NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    CBasePin*           GetPin(int n);

    //IAboutProperty
    STDMETHODIMP        GetFilterName(BSTR* pName) { *pName = m_bstrFilterName; return S_OK; };

    HRESULT             CompleteConnect(PIN_DIRECTION direction, IPin *pReceivePin);

    HRESULT             GetRequiredFramesNum(mfxU16* nMinFrames, mfxU16* nRecommendedFrames);

    //IConfigureVideoEncoder custom interface
    STDMETHODIMP        GetRunTimeStatistics(Statistics *statistics);
    STDMETHODIMP        SetParams(Params *params);
    STDMETHODIMP        GetParams(Params *params);

    //IExposeMfxMemTypeFlags custom interface
    STDMETHODIMP        GetMfxMemTypeFlags(DWORD *pFlags);

    //IShareMfxSession custom interface
    STDMETHODIMP        GetMfxSession(void **pSession);
    STDMETHODIMP        SygnalSessionHasChild() { if (m_pEncoder) {m_pEncoder->m_bSessionHasChild = true;} return S_OK; };
protected:

    virtual void            ReadFrameRateFromRegistry(mfxU32 &iFrameRate) = 0;
    virtual void            WriteParamsToRegistry() = 0;
    virtual void            WriteMfxImplToRegistry();
    virtual void            UpdateRegistry()
    {
        mfxStatus sts;

        mfxVideoParam mfxParam;
        mfxEncodeStat mfxStat;
        mfxU32        nBitrate;

        memset(&mfxParam, 0, sizeof(mfxVideoParam));
        memset(&mfxStat,  0, sizeof(mfxEncodeStat));

        sts = m_pEncoder->GetVideoParams(&mfxParam);

        if (MFX_ERR_NONE == sts)
        {
            SetParamToReg(_T("Profile"), mfxParam.mfx.CodecProfile);
            SetParamToReg(_T("Level"),   mfxParam.mfx.CodecLevel);
        }

        sts = m_pEncoder->GetEncodeStat(&mfxStat);

        if (MFX_ERR_NONE == sts && mfxStat.NumFrame)
        {
            nBitrate = (mfxU32)(mfxStat.NumBit / mfxStat.NumFrame * m_pTimeManager->GetFrameRate());

            SetParamToReg(_T("RealBitrate"),   nBitrate);
            SetParamToReg(_T("FramesEncoded"), mfxStat.NumFrame);
        }
    };

    HRESULT                 AlignFrameSizes(mfxFrameInfo* pInfo, mfxU16 nWidth, mfxU16 nHeight);
    HRESULT                 DynamicFrameSizeChange(mfxFrameInfo* pInfo);
    HRESULT                 DeliverBitstream(mfxBitstream* pBS);

    mfxStatus               OnFrameReady(mfxBitstream* pBS);
    mfxStatus               StoreEncoderParams(Params *params);
    virtual bool            EncResetRequired(const mfxFrameInfo *pNewFrameInfo);

protected:

    CComBSTR                      m_bstrFilterName;      //filter name

    BOOL                          m_bStop;               // (BOOL) graph is stopped
    CCritSec                      m_csLock;

    HANDLE                        m_Thread;

    // minimum required API version
    mfxU16                        m_nAPIVerMinor;
    mfxU16                        m_nAPIVerMajor;

    mfxVideoParam                 m_mfxParamsVideo;      // video params for ENC
    mfxVideoParam                 m_mfxParamsVPP;        // video params for VPP

    CTimeManager*                 m_pTimeManager;
    CBitstreamAdapter*            m_pAdapter;

    CBaseEncoder*                 m_pEncoder;

    BOOL                          m_bUseVPP;

    Params                        m_EncoderParams;

    DWORD                         m_FramesReceived;          // number of frames received for statistics

private:
    DISALLOW_COPY_AND_ASSIGN(CEncVideoFilter);
};

/////////////////////////////////////////////////////////////////////////////////////////////

class CEncoderInputPin : public CTransformInputPin
{
public:
    CEncoderInputPin(CTransformFilter* pTransformFilter, HRESULT* phr);
    ~CEncoderInputPin();

    // CBaseInputPin
    STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps);

    // IUnknown
    DECLARE_IUNKNOWN;
    virtual STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);

    virtual STDMETHODIMP NotifyAllocator(IMemAllocator * pAllocator, BOOL bReadOnly);

private:
    DISALLOW_COPY_AND_ASSIGN(CEncoderInputPin);
};

class CVideoMemEncoderInputPin :
    public CEncoderInputPin,
    public IMFGetService,
    public IDirectXVideoMemoryConfiguration
{
public:
    CVideoMemEncoderInputPin(CTransformFilter* pTransformFilter, HRESULT* phr);
    ~CVideoMemEncoderInputPin();

    // IUnknown
    DECLARE_IUNKNOWN;
    virtual STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);

    // IDirectXVideoMemoryConfiguration
    virtual STDMETHODIMP GetService(REFGUID guidService,
        REFIID riid,
        LPVOID *ppvObject);
    virtual STDMETHODIMP GetAvailableSurfaceTypeByIndex(
        DWORD dwTypeIndex,
        DXVA2_SurfaceType *pdwType);
    virtual STDMETHODIMP SetSurfaceType(DXVA2_SurfaceType dwType);

protected:
    IDirect3D9Ex*            m_pd3d;
    IDirect3DDeviceManager9* m_pd3dDeviceManager;
    IDirect3DDevice9Ex*      m_pd3dDevice;

private:
    DISALLOW_COPY_AND_ASSIGN(CVideoMemEncoderInputPin);
};
