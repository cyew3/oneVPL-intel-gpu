/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include <d3d11.h>
#include "dxva2_spy.h"
#include "dxva2_log.h"

class SpyD3D10Multithreaded : public ID3D10Multithread
{
    ID3D10Multithread * m_pObject;
public:
    SpyD3D10Multithreaded(ID3D10Multithread * pObject)
        : m_pObject(pObject)
        , m_nRefCount(1)
    {
    }
    INKNOWN_IMPL(m_pObject);

    virtual void STDMETHODCALLTYPE Enter( void)
    {
        LLL;
        m_pObject->Enter();
    }

    virtual void STDMETHODCALLTYPE Leave( void)
    {
        LLL;
        m_pObject->Leave();
    }

    virtual BOOL STDMETHODCALLTYPE SetMultithreadProtected(
        _In_  BOOL bMTProtect)
    {
        LLL;
        logi(bMTProtect);
        return m_pObject->SetMultithreadProtected(bMTProtect);
    }

    virtual BOOL STDMETHODCALLTYPE GetMultithreadProtected( void)
    {
        LLL;
        BOOL bProtected = m_pObject->GetMultithreadProtected();
        logi(bProtected);
        return bProtected;
    }
};

class SpyD3D11VideoDecoder : public ID3D11VideoDecoder
{
    ID3D11VideoDecoder * m_pObject;

public:
    SpyD3D11VideoDecoder(ID3D11VideoDecoder * pDecoder)
        : m_pObject(pDecoder)
        , m_nRefCount(1)
    {
    }

    INKNOWN_IMPL_REF(m_pObject);

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **pp)
    {
        LLL;
        LOG_GUID(id);
        HRESULT hr = m_pObject->QueryInterface(id, pp);
        logi(hr);
        return hr;
    }

    virtual HRESULT STDMETHODCALLTYPE GetCreationParameters(
        _Out_  D3D11_VIDEO_DECODER_DESC *pVideoDesc,
        _Out_  D3D11_VIDEO_DECODER_CONFIG *pConfig)
    {
        LLL;
        return m_pObject->GetCreationParameters(pVideoDesc, pConfig);
    }

    virtual HRESULT STDMETHODCALLTYPE GetDriverHandle(
        _Out_  HANDLE *pDriverHandle)
    {
        LLL;
        return m_pObject->GetDriverHandle(pDriverHandle);
    }

    virtual void STDMETHODCALLTYPE GetDevice(
        _Out_  ID3D11Device **ppDevice)
    {
        LLL;
        return m_pObject->GetDevice(ppDevice);
    }

    virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
        _In_  REFGUID guid,
        _Inout_  UINT *pDataSize,
        _Out_writes_bytes_opt_( *pDataSize )  void *pData)
    {
        LLL;
        return m_pObject->GetPrivateData(guid, pDataSize, pData);
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
        _In_  REFGUID guid,
        _In_  UINT DataSize,
        _In_reads_bytes_opt_( DataSize )  const void *pData)
    {
        LLL;
        return m_pObject->SetPrivateData(guid, DataSize, pData);
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        _In_  REFGUID guid,
        _In_opt_  const IUnknown *pData)
    {
        LLL;
        return m_pObject->SetPrivateDataInterface(guid, pData);
    }

};

class SpyD3D11VideoDevice : public ID3D11VideoDevice
{
    ID3D11VideoDevice *m_pObject;

public:
    SpyD3D11VideoDevice(ID3D11VideoDevice *pTarget)
        : m_pObject(pTarget)
        , m_nRefCount(1)
    {
        LLL;
    }
    virtual ~SpyD3D11VideoDevice()
    {
    }
    INKNOWN_IMPL_REF(m_pObject);

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **pp)
    {
        LLL;
        LOG_GUID(id);
        HRESULT hr = m_pObject->QueryInterface(id, pp);
        logi(hr);
        return hr;
    }

    virtual HRESULT STDMETHODCALLTYPE CreateVideoDecoder(
        const D3D11_VIDEO_DECODER_DESC *pVideoDesc,
        const D3D11_VIDEO_DECODER_CONFIG *pConfig,
        ID3D11VideoDecoder **ppDecoder);

    virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessor(
        _In_  ID3D11VideoProcessorEnumerator *pEnum,
        _In_  UINT RateConversionIndex,
        _Out_  ID3D11VideoProcessor **ppVideoProcessor)
    {
        LLL;
        return m_pObject->CreateVideoProcessor(pEnum, RateConversionIndex, ppVideoProcessor);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateAuthenticatedChannel(
        _In_  D3D11_AUTHENTICATED_CHANNEL_TYPE ChannelType,
        _Out_  ID3D11AuthenticatedChannel **ppAuthenticatedChannel)
    {
        LLL;
        return m_pObject->CreateAuthenticatedChannel(ChannelType, ppAuthenticatedChannel);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateCryptoSession(
        _In_  const GUID *pCryptoType,
        _In_opt_  const GUID *pDecoderProfile,
        _In_  const GUID *pKeyExchangeType,
        _Outptr_  ID3D11CryptoSession **ppCryptoSession)
    {
        LLL;
        return m_pObject->CreateCryptoSession(pCryptoType, pDecoderProfile, pKeyExchangeType, ppCryptoSession);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateVideoDecoderOutputView(
        _In_  ID3D11Resource *pResource,
        _In_  const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc,
        _Out_opt_  ID3D11VideoDecoderOutputView **ppVDOVView)
    {
        LLL;
        return m_pObject->CreateVideoDecoderOutputView( pResource, pDesc, ppVDOVView);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessorInputView(
        _In_  ID3D11Resource *pResource,
        _In_  ID3D11VideoProcessorEnumerator *pEnum,
        _In_  const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC *pDesc,
        _Out_opt_  ID3D11VideoProcessorInputView **ppVPIView)
    {
        return m_pObject->CreateVideoProcessorInputView( pResource, pEnum, pDesc, ppVPIView);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessorOutputView(
        _In_  ID3D11Resource *pResource,
        _In_  ID3D11VideoProcessorEnumerator *pEnum,
        _In_  const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC *pDesc,
        _Out_opt_  ID3D11VideoProcessorOutputView **ppVPOView)
    {
        LLL;
        return m_pObject->CreateVideoProcessorOutputView(  pResource, pEnum, pDesc, ppVPOView);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessorEnumerator(
        _In_  const D3D11_VIDEO_PROCESSOR_CONTENT_DESC *pDesc,
        _Out_  ID3D11VideoProcessorEnumerator **ppEnum)
    {
        LLL;
        return m_pObject->CreateVideoProcessorEnumerator( pDesc, ppEnum);
    }
    virtual UINT STDMETHODCALLTYPE GetVideoDecoderProfileCount( void)
    {
        LLL;
        UINT n = m_pObject->GetVideoDecoderProfileCount();
        logi(n);
        return n;
    }

    virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderProfile(
        _In_  UINT Index,
        _Out_  GUID *pDecoderProfile)
    {
        LLL;
        logi(Index);
        return m_pObject->GetVideoDecoderProfile( Index, pDecoderProfile);
    }

    virtual HRESULT STDMETHODCALLTYPE CheckVideoDecoderFormat(
        _In_  const GUID *pDecoderProfile,
        _In_  DXGI_FORMAT Format,
        _Out_  BOOL *pSupported)
    {
        LLL;
        return m_pObject->CheckVideoDecoderFormat( pDecoderProfile, Format, pSupported);
    }

    virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderConfigCount(
        _In_  const D3D11_VIDEO_DECODER_DESC *pDesc,
        _Out_  UINT *pCount)
    {
        LLL;
        HRESULT hr = m_pObject->GetVideoDecoderConfigCount( pDesc, pCount);
        logi(*pCount);
        return hr;
    }

    virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderConfig(
        _In_  const D3D11_VIDEO_DECODER_DESC *pDesc,
        _In_  UINT Index,
        _Out_  D3D11_VIDEO_DECODER_CONFIG *pConfig)
    {
        LLL;
        return m_pObject->GetVideoDecoderConfig( pDesc, Index, pConfig);
    }

    virtual HRESULT STDMETHODCALLTYPE GetContentProtectionCaps(
        _In_opt_  const GUID *pCryptoType,
        _In_opt_  const GUID *pDecoderProfile,
        _Out_  D3D11_VIDEO_CONTENT_PROTECTION_CAPS *pCaps)
    {
        LLL;
        return m_pObject->GetContentProtectionCaps( pCryptoType, pDecoderProfile, pCaps);
    }

    virtual HRESULT STDMETHODCALLTYPE CheckCryptoKeyExchange(
        _In_  const GUID *pCryptoType,
        _In_opt_  const GUID *pDecoderProfile,
        _In_  UINT Index,
        _Out_  GUID *pKeyExchangeType)
    {
        LLL;
        return m_pObject->CheckCryptoKeyExchange( pCryptoType, pDecoderProfile, Index, pKeyExchangeType);
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
        _In_  REFGUID guid,
        _In_  UINT DataSize,
        _In_reads_bytes_opt_(DataSize)  const void *pData)
    {
        LLL;
        return m_pObject->SetPrivateData( guid, DataSize, pData);
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        _In_  REFGUID guid,
        _In_opt_  const IUnknown *pData)
    {
        LLL;
        return m_pObject->SetPrivateDataInterface( guid, pData);
    }
};

class SpyD3D11VideoContext : public ID3D11VideoContext
{
private:
    ID3D11VideoContext *m_pObject;

public:
    SpyD3D11VideoContext(ID3D11VideoContext *pTarget)
        : m_pObject(pTarget)
        , m_nRefCount(1)
    {
        LLL;
    }
    virtual ~SpyD3D11VideoContext()
    {
    }
    INKNOWN_IMPL(m_pObject);

        /// idevchild methods
        virtual void STDMETHODCALLTYPE GetDevice(
        /* [annotation] */
        _Out_  ID3D11Device **ppDevice) {
        LLL;
        return m_pObject->GetDevice(ppDevice);
    }
    virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
        /* [annotation] */
        _In_  REFGUID guid,
        /* [annotation] */
        _Inout_  UINT *pDataSize,
        /* [annotation] */
        _Out_writes_bytes_opt_( *pDataSize )  void *pData){
        LLL;
        return m_pObject->GetPrivateData(guid, pDataSize, pData);
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
        /* [annotation] */
        _In_  REFGUID guid,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _In_reads_bytes_opt_( DataSize )  const void *pData) {
        LLL;
        return m_pObject->SetPrivateData(guid, DataSize, pData);
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        /* [annotation] */
        _In_  REFGUID guid,
        /* [annotation] */
        _In_opt_  const IUnknown *pData) {
        LLL;
        return m_pObject->SetPrivateDataInterface(guid, pData);
    }

    virtual HRESULT STDMETHODCALLTYPE GetDecoderBuffer(
        /* [annotation] */
        _In_  ID3D11VideoDecoder *pDecoder,
        /* [annotation] */
        _In_  D3D11_VIDEO_DECODER_BUFFER_TYPE Type,
        /* [annotation] */
        _Out_  UINT *pBufferSize,
        /* [annotation] */
        _Out_writes_bytes_opt_(*pBufferSize)  void **ppBuffer);

    virtual HRESULT STDMETHODCALLTYPE ReleaseDecoderBuffer(
        /* [annotation] */
        _In_  ID3D11VideoDecoder *pDecoder,
        /* [annotation] */
        _In_  D3D11_VIDEO_DECODER_BUFFER_TYPE Type)
    {
        LLL;
        return m_pObject->ReleaseDecoderBuffer(pDecoder,Type);
    }

    virtual HRESULT STDMETHODCALLTYPE DecoderBeginFrame(
        /* [annotation] */
        _In_  ID3D11VideoDecoder *pDecoder,
        /* [annotation] */
        _In_  ID3D11VideoDecoderOutputView *pView,
        /* [annotation] */
        _In_  UINT ContentKeySize,
        /* [annotation] */
        _In_reads_bytes_opt_(ContentKeySize)  const void *pContentKey);

    virtual HRESULT STDMETHODCALLTYPE DecoderEndFrame(
        /* [annotation] */
        _In_  ID3D11VideoDecoder *pDecoder);

    virtual HRESULT STDMETHODCALLTYPE SubmitDecoderBuffers(
        /* [annotation] */
        _In_  ID3D11VideoDecoder *pDecoder,
        /* [annotation] */
        _In_  UINT NumBuffers,
        /* [annotation] */
        _In_reads_(NumBuffers)  const D3D11_VIDEO_DECODER_BUFFER_DESC *pBufferDesc);

    virtual HRESULT STDMETHODCALLTYPE DecoderExtension(
        /* [annotation] */
        _In_  ID3D11VideoDecoder *pDecoder,
        /* [annotation] */
        _In_  const D3D11_VIDEO_DECODER_EXTENSION *pExtensionData)
    {
        //LLL;
        return m_pObject->DecoderExtension(pDecoder, pExtensionData);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetOutputTargetRect(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_opt_  const RECT *pRect)
    {
        LLL;
        return m_pObject->VideoProcessorSetOutputTargetRect(pVideoProcessor, Enable, pRect);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetOutputBackgroundColor(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  BOOL YCbCr,
        /* [annotation] */
        _In_  const D3D11_VIDEO_COLOR *pColor)
    {
        LLL;
        return m_pObject->VideoProcessorSetOutputBackgroundColor(pVideoProcessor, YCbCr, pColor);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetOutputColorSpace(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
    {
        LLL;
        return m_pObject->VideoProcessorSetOutputColorSpace(pVideoProcessor, pColorSpace);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetOutputAlphaFillMode(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode,
        /* [annotation] */
        _In_  UINT StreamIndex)
    {
        LLL;
        return m_pObject->VideoProcessorSetOutputAlphaFillMode(pVideoProcessor, AlphaFillMode, StreamIndex);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetOutputConstriction(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_  SIZE Size)
    {
        LLL;
        return m_pObject->VideoProcessorSetOutputConstriction(pVideoProcessor, Enable, Size);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetOutputStereoMode(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  BOOL Enable)
    {
        LLL;
        return m_pObject->VideoProcessorSetOutputStereoMode(pVideoProcessor, Enable);
    }

    virtual HRESULT STDMETHODCALLTYPE VideoProcessorSetOutputExtension(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  const GUID *pExtensionGuid,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _In_  void *pData)
    {
        LLL;
        return m_pObject->VideoProcessorSetOutputExtension(
pVideoProcessor,
pExtensionGuid,
DataSize,
pData
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetOutputTargetRect(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _Out_  BOOL *Enabled,
        /* [annotation] */
        _Out_  RECT *pRect)
    {
        LLL;
        return m_pObject->VideoProcessorGetOutputTargetRect(pVideoProcessor, Enabled, pRect);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetOutputBackgroundColor(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _Out_  BOOL *pYCbCr,
        /* [annotation] */
        _Out_  D3D11_VIDEO_COLOR *pColor)
    {
        LLL;
        return m_pObject->VideoProcessorGetOutputBackgroundColor(
pVideoProcessor,
pYCbCr,
pColor
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetOutputColorSpace(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _Out_  D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
    {
        LLL;
        return m_pObject->VideoProcessorGetOutputColorSpace(
pVideoProcessor,
pColorSpace
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetOutputAlphaFillMode(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _Out_  D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE *pAlphaFillMode,
        /* [annotation] */
        _Out_  UINT *pStreamIndex)
    {
        LLL;
        return m_pObject->VideoProcessorGetOutputAlphaFillMode(
pVideoProcessor,
pAlphaFillMode,
pStreamIndex
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetOutputConstriction(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _Out_  BOOL *pEnabled,
        /* [annotation] */
        _Out_  SIZE *pSize)
    {
        LLL;
        return m_pObject->VideoProcessorGetOutputConstriction(
pVideoProcessor,
pEnabled,
pSize
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetOutputStereoMode(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _Out_  BOOL *pEnabled)
    {
        LLL;
        return m_pObject->VideoProcessorGetOutputStereoMode(
pVideoProcessor,
pEnabled
            );
    }

    virtual HRESULT STDMETHODCALLTYPE VideoProcessorGetOutputExtension(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  const GUID *pExtensionGuid,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _Out_  void *pData)
    {
        LLL;
        return m_pObject->VideoProcessorGetOutputExtension(pVideoProcessor, pExtensionGuid, DataSize, pData);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamFrameFormat(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  D3D11_VIDEO_FRAME_FORMAT FrameFormat)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamFrameFormat(pVideoProcessor, StreamIndex, FrameFormat);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamColorSpace(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamColorSpace(pVideoProcessor, StreamIndex, pColorSpace);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamOutputRate(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  D3D11_VIDEO_PROCESSOR_OUTPUT_RATE OutputRate,
        /* [annotation] */
        _In_  BOOL RepeatFrame,
        /* [annotation] */
        _In_opt_  const DXGI_RATIONAL *pCustomRate)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamOutputRate(pVideoProcessor, StreamIndex, OutputRate, RepeatFrame, pCustomRate);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamSourceRect(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_opt_  const RECT *pRect)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamSourceRect(pVideoProcessor, StreamIndex, Enable, pRect);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamDestRect(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_opt_  const RECT *pRect)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamDestRect(pVideoProcessor, StreamIndex, Enable, pRect);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamAlpha(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_  FLOAT Alpha)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamAlpha(pVideoProcessor, StreamIndex, Enable, Alpha);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamPalette(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  UINT Count,
        /* [annotation] */
        _In_reads_opt_(Count)  const UINT *pEntries)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamPalette(pVideoProcessor, StreamIndex, Count, pEntries);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamPixelAspectRatio(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_opt_  const DXGI_RATIONAL *pSourceAspectRatio,
        /* [annotation] */
        _In_opt_  const DXGI_RATIONAL *pDestinationAspectRatio)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamPixelAspectRatio(pVideoProcessor, StreamIndex, Enable, pSourceAspectRatio, pDestinationAspectRatio);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamLumaKey(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_  FLOAT Lower,
        /* [annotation] */
        _In_  FLOAT Upper)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamLumaKey(pVideoProcessor, StreamIndex, Enable, Lower, Upper);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamStereoFormat(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_  D3D11_VIDEO_PROCESSOR_STEREO_FORMAT Format,
        /* [annotation] */
        _In_  BOOL LeftViewFrame0,
        /* [annotation] */
        _In_  BOOL BaseViewFrame0,
        /* [annotation] */
        _In_  D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE FlipMode,
        /* [annotation] */
        _In_  int MonoOffset)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamStereoFormat(
pVideoProcessor,
StreamIndex,
Enable,
Format,
LeftViewFrame0,
BaseViewFrame0,
FlipMode,
MonoOffset
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamAutoProcessingMode(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  BOOL Enable)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamAutoProcessingMode(pVideoProcessor, StreamIndex, Enable);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamFilter(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  D3D11_VIDEO_PROCESSOR_FILTER Filter,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_  int Level)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamFilter(pVideoProcessor, StreamIndex, Filter, Enable, Level);
    }

    virtual HRESULT STDMETHODCALLTYPE VideoProcessorSetStreamExtension(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  const GUID *pExtensionGuid,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _In_  void *pData)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamExtension(
pVideoProcessor,
StreamIndex,
pExtensionGuid,
DataSize,
pData
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamFrameFormat(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  D3D11_VIDEO_FRAME_FORMAT *pFrameFormat)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamFrameFormat(pVideoProcessor, StreamIndex, pFrameFormat);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamColorSpace(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamColorSpace(pVideoProcessor, StreamIndex, pColorSpace);
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamOutputRate(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  D3D11_VIDEO_PROCESSOR_OUTPUT_RATE *pOutputRate,
        /* [annotation] */
        _Out_  BOOL *pRepeatFrame,
        /* [annotation] */
        _Out_  DXGI_RATIONAL *pCustomRate)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamOutputRate(
pVideoProcessor,
StreamIndex,
pOutputRate,
pRepeatFrame,
pCustomRate
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamSourceRect(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  BOOL *pEnabled,
        /* [annotation] */
        _Out_  RECT *pRect)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamSourceRect(
pVideoProcessor,
StreamIndex,
pEnabled,
pRect
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamDestRect(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  BOOL *pEnabled,
        /* [annotation] */
        _Out_  RECT *pRect)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamDestRect(
pVideoProcessor,
StreamIndex,
pEnabled,
pRect
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamAlpha(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  BOOL *pEnabled,
        /* [annotation] */
        _Out_  FLOAT *pAlpha)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamAlpha(
pVideoProcessor,
StreamIndex,
pEnabled,
pAlpha
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamPalette(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  UINT Count,
        /* [annotation] */
        _Out_writes_(Count)  UINT *pEntries)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamPalette(
pVideoProcessor,
StreamIndex,
Count,
pEntries
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamPixelAspectRatio(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  BOOL *pEnabled,
        /* [annotation] */
        _Out_  DXGI_RATIONAL *pSourceAspectRatio,
        /* [annotation] */
        _Out_  DXGI_RATIONAL *pDestinationAspectRatio)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamPixelAspectRatio(
pVideoProcessor,
StreamIndex,
pEnabled,
pSourceAspectRatio,
pDestinationAspectRatio
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamLumaKey(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  BOOL *pEnabled,
        /* [annotation] */
        _Out_  FLOAT *pLower,
        /* [annotation] */
        _Out_  FLOAT *pUpper)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamLumaKey(
pVideoProcessor,
StreamIndex,
pEnabled,
pLower,
pUpper
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamStereoFormat(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  BOOL *pEnable,
        /* [annotation] */
        _Out_  D3D11_VIDEO_PROCESSOR_STEREO_FORMAT *pFormat,
        /* [annotation] */
        _Out_  BOOL *pLeftViewFrame0,
        /* [annotation] */
        _Out_  BOOL *pBaseViewFrame0,
        /* [annotation] */
        _Out_  D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE *pFlipMode,
        /* [annotation] */
        _Out_  int *MonoOffset)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamStereoFormat(
pVideoProcessor,
StreamIndex,
pEnable,
pFormat,
pLeftViewFrame0,
pBaseViewFrame0,
pFlipMode,
MonoOffset
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamAutoProcessingMode(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  BOOL *pEnabled)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamAutoProcessingMode(
pVideoProcessor,
StreamIndex,
pEnabled
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamFilter(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  D3D11_VIDEO_PROCESSOR_FILTER Filter,
        /* [annotation] */
        _Out_  BOOL *pEnabled,
        /* [annotation] */
        _Out_  int *pLevel)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamFilter(
pVideoProcessor,
StreamIndex,
Filter,
pEnabled,
pLevel
            );
    }

    virtual HRESULT STDMETHODCALLTYPE VideoProcessorGetStreamExtension(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  const GUID *pExtensionGuid,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _Out_  void *pData)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamExtension(
pVideoProcessor,
StreamIndex,
pExtensionGuid,
DataSize,
pData
            );
    }

    virtual HRESULT STDMETHODCALLTYPE VideoProcessorBlt(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  ID3D11VideoProcessorOutputView *pView,
        /* [annotation] */
        _In_  UINT OutputFrame,
        /* [annotation] */
        _In_  UINT StreamCount,
        /* [annotation] */
        _In_reads_(StreamCount)  const D3D11_VIDEO_PROCESSOR_STREAM *pStreams)
    {
        LLL;
        return m_pObject->VideoProcessorBlt(
pVideoProcessor,
pView,
OutputFrame,
StreamCount,
pStreams
            );
    }

    virtual HRESULT STDMETHODCALLTYPE NegotiateCryptoSessionKeyExchange(
        /* [annotation] */
        _In_  ID3D11CryptoSession *pCryptoSession,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _Inout_updates_bytes_(DataSize)  void *pData)
    {
        LLL;
        return m_pObject->NegotiateCryptoSessionKeyExchange(
pCryptoSession,
DataSize,
pData
            );
    }

    virtual void STDMETHODCALLTYPE EncryptionBlt(
        /* [annotation] */
        _In_  ID3D11CryptoSession *pCryptoSession,
        /* [annotation] */
        _In_  ID3D11Texture2D *pSrcSurface,
        /* [annotation] */
        _In_  ID3D11Texture2D *pDstSurface,
        /* [annotation] */
        _In_  UINT IVSize,
        /* [annotation] */
        _In_reads_bytes_opt_(IVSize)  void *pIV)
    {
        LLL;
        return m_pObject->EncryptionBlt(
pCryptoSession,
pSrcSurface,
pDstSurface,
IVSize,
pIV
            );
    }

    virtual void STDMETHODCALLTYPE DecryptionBlt(
        /* [annotation] */
        _In_  ID3D11CryptoSession *pCryptoSession,
        /* [annotation] */
        _In_  ID3D11Texture2D *pSrcSurface,
        /* [annotation] */
        _In_  ID3D11Texture2D *pDstSurface,
        /* [annotation] */
        _In_opt_  D3D11_ENCRYPTED_BLOCK_INFO *pEncryptedBlockInfo,
        /* [annotation] */
        _In_  UINT ContentKeySize,
        /* [annotation] */
        _In_reads_bytes_opt_(ContentKeySize)  const void *pContentKey,
        /* [annotation] */
        _In_  UINT IVSize,
        /* [annotation] */
        _In_reads_bytes_opt_(IVSize)  void *pIV)
    {
        LLL;
        return m_pObject->DecryptionBlt(
pCryptoSession,
pSrcSurface,
pDstSurface,
pEncryptedBlockInfo,
ContentKeySize,
pContentKey,
IVSize,
pIV
            );
    }

    virtual void STDMETHODCALLTYPE StartSessionKeyRefresh(
        /* [annotation] */
        _In_  ID3D11CryptoSession *pCryptoSession,
        /* [annotation] */
        _In_  UINT RandomNumberSize,
        /* [annotation] */
        _Out_writes_bytes_(RandomNumberSize)  void *pRandomNumber)
    {
        LLL;
        return m_pObject->StartSessionKeyRefresh(
pCryptoSession,
RandomNumberSize,
pRandomNumber
            );
    }

    virtual void STDMETHODCALLTYPE FinishSessionKeyRefresh(
        /* [annotation] */
        _In_  ID3D11CryptoSession *pCryptoSession)
    {
        LLL;
        return m_pObject->FinishSessionKeyRefresh(
pCryptoSession
            );
    }

    virtual HRESULT STDMETHODCALLTYPE GetEncryptionBltKey(
        /* [annotation] */
        _In_  ID3D11CryptoSession *pCryptoSession,
        /* [annotation] */
        _In_  UINT KeySize,
        /* [annotation] */
        _Out_writes_bytes_(KeySize)  void *pReadbackKey)
    {
        LLL;
        return m_pObject->GetEncryptionBltKey(
pCryptoSession,
KeySize,
pReadbackKey
            );
    }

    virtual HRESULT STDMETHODCALLTYPE NegotiateAuthenticatedChannelKeyExchange(
        /* [annotation] */
        _In_  ID3D11AuthenticatedChannel *pChannel,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _Inout_updates_bytes_(DataSize)  void *pData)
    {
        LLL;
        return m_pObject->NegotiateAuthenticatedChannelKeyExchange(
pChannel,
DataSize,
pData
            );
    }

    virtual HRESULT STDMETHODCALLTYPE QueryAuthenticatedChannel(
        /* [annotation] */
        _In_  ID3D11AuthenticatedChannel *pChannel,
        /* [annotation] */
        _In_  UINT InputSize,
        /* [annotation] */
        _In_reads_bytes_(InputSize)  const void *pInput,
        /* [annotation] */
        _In_  UINT OutputSize,
        /* [annotation] */
        _Out_writes_bytes_(OutputSize)  void *pOutput)
    {
        LLL;
        return m_pObject->QueryAuthenticatedChannel(
pChannel,
InputSize,
pInput,
OutputSize,
pOutput
            );
    }

    virtual HRESULT STDMETHODCALLTYPE ConfigureAuthenticatedChannel(
        /* [annotation] */
        _In_  ID3D11AuthenticatedChannel *pChannel,
        /* [annotation] */
        _In_  UINT InputSize,
        /* [annotation] */
        _In_reads_bytes_(InputSize)  const void *pInput,
        /* [annotation] */
        _Out_  D3D11_AUTHENTICATED_CONFIGURE_OUTPUT *pOutput)
    {
        LLL;
        return m_pObject->ConfigureAuthenticatedChannel(
pChannel,
InputSize,
pInput,
pOutput
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorSetStreamRotation(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _In_  BOOL Enable,
        /* [annotation] */
        _In_  D3D11_VIDEO_PROCESSOR_ROTATION Rotation)
    {
        LLL;
        return m_pObject->VideoProcessorSetStreamRotation(
pVideoProcessor,
StreamIndex,
Enable,
Rotation
            );
    }

    virtual void STDMETHODCALLTYPE VideoProcessorGetStreamRotation(
        /* [annotation] */
        _In_  ID3D11VideoProcessor *pVideoProcessor,
        /* [annotation] */
        _In_  UINT StreamIndex,
        /* [annotation] */
        _Out_  BOOL *pEnable,
        /* [annotation] */
        _Out_  D3D11_VIDEO_PROCESSOR_ROTATION *pRotation)
    {
        LLL;
        return m_pObject->VideoProcessorGetStreamRotation(
pVideoProcessor,
StreamIndex,
pEnable,
pRotation
            );
    }

};

class SpyD3D11DeviceContext : public ID3D11DeviceContext
{
private:
    ID3D11DeviceContext *m_pObject;
public:
    SpyD3D11DeviceContext(ID3D11DeviceContext * pObj)
        : m_pObject(pObj)
        , m_nRefCount(1)
    {
    }
    virtual ~SpyD3D11DeviceContext()
    {
    }
    INKNOWN_IMPL_REF(m_pObject);
    STDMETHODIMP QueryInterface(REFIID id, void **pp)
    {
        if (id == __uuidof(ID3D11VideoContext) && pp)
        {
            ID3D11VideoContext *pIvidctx;
            HRESULT hr = m_pObject->QueryInterface(id, (void**)&pIvidctx);
            if (FAILED(hr))
                return hr;

            *pp = new SpyD3D11VideoContext(pIvidctx);

            return hr;
        }
        return m_pObject->QueryInterface(id, pp);
    }
    /// idevchild methods
        virtual void STDMETHODCALLTYPE GetDevice(
        /* [annotation] */
        _Out_  ID3D11Device **ppDevice) {
        LLL;
        return m_pObject->GetDevice(ppDevice);
    }
    virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
        /* [annotation] */
        _In_  REFGUID guid,
        /* [annotation] */
        _Inout_  UINT *pDataSize,
        /* [annotation] */
        _Out_writes_bytes_opt_( *pDataSize )  void *pData){
        LLL;
        return m_pObject->GetPrivateData(
guid,
pDataSize,
pData);
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
        /* [annotation] */
        _In_  REFGUID guid,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _In_reads_bytes_opt_( DataSize )  const void *pData) {
        LLL;
        return m_pObject->SetPrivateData(
guid,
DataSize,
pData
            );
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        /* [annotation] */
        _In_  REFGUID guid,
        /* [annotation] */
        _In_opt_  const IUnknown *pData) {
        LLL;
        return m_pObject->SetPrivateDataInterface(
guid,
pData
            );
    }

    virtual void STDMETHODCALLTYPE VSSetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
    {
        LLL;
        return m_pObject->VSSetConstantBuffers(
StartSlot,
NumBuffers,
ppConstantBuffers
            );
    }

    virtual void STDMETHODCALLTYPE PSSetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
    {
        LLL;
        return m_pObject->PSSetShaderResources(
StartSlot,
NumViews,
ppShaderResourceViews
            );
    }

    virtual void STDMETHODCALLTYPE PSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11PixelShader *pPixelShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances)
    {
        LLL;
        return m_pObject->PSSetShader(
pPixelShader,
ppClassInstances,
NumClassInstances
            );
    }

    virtual void STDMETHODCALLTYPE PSSetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
    {
        LLL;
        return m_pObject->PSSetSamplers(
StartSlot,
NumSamplers,
ppSamplers
            );
    }

    virtual void STDMETHODCALLTYPE VSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11VertexShader *pVertexShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances)
    {
        LLL;
        return m_pObject->VSSetShader(
pVertexShader,
ppClassInstances,
NumClassInstances
            );
    }

    virtual void STDMETHODCALLTYPE DrawIndexed(
        /* [annotation] */
        _In_  UINT IndexCount,
        /* [annotation] */
        _In_  UINT StartIndexLocation,
        /* [annotation] */
        _In_  INT BaseVertexLocation)
    {
        LLL;
        return m_pObject->DrawIndexed(
IndexCount,
StartIndexLocation,
BaseVertexLocation
            );
    }

    virtual void STDMETHODCALLTYPE Draw(
        /* [annotation] */
        _In_  UINT VertexCount,
        /* [annotation] */
        _In_  UINT StartVertexLocation)
    {
        LLL;
        return m_pObject->Draw(
VertexCount,
StartVertexLocation
            );
    }

    virtual HRESULT STDMETHODCALLTYPE Map(
        /* [annotation] */
        _In_  ID3D11Resource *pResource,
        /* [annotation] */
        _In_  UINT Subresource,
        /* [annotation] */
        _In_  D3D11_MAP MapType,
        /* [annotation] */
        _In_  UINT MapFlags,
        /* [annotation] */
        _Out_  D3D11_MAPPED_SUBRESOURCE *pMappedResource)
    {
        LLL;
        return m_pObject->Map(
pResource,
Subresource,
MapType,
MapFlags,
pMappedResource
            );
    }

    virtual void STDMETHODCALLTYPE Unmap(
        /* [annotation] */
        _In_  ID3D11Resource *pResource,
        /* [annotation] */
        _In_  UINT Subresource)
    {
        LLL;
        return m_pObject->Unmap(
pResource,
Subresource
            );
    }

    virtual void STDMETHODCALLTYPE PSSetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
    {
        LLL;
        return m_pObject->PSSetConstantBuffers(
StartSlot,
NumBuffers,
ppConstantBuffers
            );
    }

    virtual void STDMETHODCALLTYPE IASetInputLayout(
        /* [annotation] */
        _In_opt_  ID3D11InputLayout *pInputLayout)
    {
        LLL;
        return m_pObject->IASetInputLayout(
pInputLayout
            );
    }

    virtual void STDMETHODCALLTYPE IASetVertexBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppVertexBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  const UINT *pStrides,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  const UINT *pOffsets)
    {
        LLL;
        return m_pObject->IASetVertexBuffers(
StartSlot,
NumBuffers,
ppVertexBuffers,
pStrides,
pOffsets
            );
    }

    virtual void STDMETHODCALLTYPE IASetIndexBuffer(
        /* [annotation] */
        _In_opt_  ID3D11Buffer *pIndexBuffer,
        /* [annotation] */
        _In_  DXGI_FORMAT Format,
        /* [annotation] */
        _In_  UINT Offset)
    {
        LLL;
        return m_pObject->IASetIndexBuffer(
pIndexBuffer,
Format,
Offset
            );
    }

    virtual void STDMETHODCALLTYPE DrawIndexedInstanced(
        /* [annotation] */
        _In_  UINT IndexCountPerInstance,
        /* [annotation] */
        _In_  UINT InstanceCount,
        /* [annotation] */
        _In_  UINT StartIndexLocation,
        /* [annotation] */
        _In_  INT BaseVertexLocation,
        /* [annotation] */
        _In_  UINT StartInstanceLocation)
    {
        LLL;
        return m_pObject->DrawIndexedInstanced(
IndexCountPerInstance,
InstanceCount,
StartIndexLocation,
BaseVertexLocation,
StartInstanceLocation
            );
    }

    virtual void STDMETHODCALLTYPE DrawInstanced(
        /* [annotation] */
        _In_  UINT VertexCountPerInstance,
        /* [annotation] */
        _In_  UINT InstanceCount,
        /* [annotation] */
        _In_  UINT StartVertexLocation,
        /* [annotation] */
        _In_  UINT StartInstanceLocation)
    {
        LLL;
        return m_pObject->DrawInstanced(
VertexCountPerInstance,
InstanceCount,
StartVertexLocation,
StartInstanceLocation
            );
    }

    virtual void STDMETHODCALLTYPE GSSetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
    {
        LLL;
        return m_pObject->GSSetConstantBuffers(
StartSlot,
NumBuffers,
ppConstantBuffers
            );
    }

    virtual void STDMETHODCALLTYPE GSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11GeometryShader *pShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances)
    {
        LLL;
        return m_pObject->GSSetShader(
pShader,
ppClassInstances,
NumClassInstances
            );
    }

    virtual void STDMETHODCALLTYPE IASetPrimitiveTopology(
        /* [annotation] */
        _In_  D3D11_PRIMITIVE_TOPOLOGY Topology)
    {
        LLL;
        return m_pObject->IASetPrimitiveTopology(
Topology
            );
    }

    virtual void STDMETHODCALLTYPE VSSetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
    {
        LLL;
        return m_pObject->VSSetShaderResources(
StartSlot,
NumViews,
ppShaderResourceViews
            );
    }

    virtual void STDMETHODCALLTYPE VSSetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
    {
        LLL;
        return m_pObject->VSSetSamplers(
StartSlot,
NumSamplers,
ppSamplers
            );
    }

    virtual void STDMETHODCALLTYPE Begin(
        /* [annotation] */
        _In_  ID3D11Asynchronous *pAsync)
    {
        LLL;
        return m_pObject->Begin(
pAsync
            );
    }

    virtual void STDMETHODCALLTYPE End(
        /* [annotation] */
        _In_  ID3D11Asynchronous *pAsync)
    {
        LLL;
        return m_pObject->End(
pAsync
            );
    }

    virtual HRESULT STDMETHODCALLTYPE GetData(
        /* [annotation] */
        _In_  ID3D11Asynchronous *pAsync,
        /* [annotation] */
        _Out_writes_bytes_opt_( DataSize )  void *pData,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _In_  UINT GetDataFlags)
    {
        LLL;
        return m_pObject->GetData(
pAsync,
pData,
DataSize,
GetDataFlags
            );
    }

    virtual void STDMETHODCALLTYPE SetPredication(
        /* [annotation] */
        _In_opt_  ID3D11Predicate *pPredicate,
        /* [annotation] */
        _In_  BOOL PredicateValue)
    {
        LLL;
        return m_pObject->SetPredication(
pPredicate,
PredicateValue
            );
    }

    virtual void STDMETHODCALLTYPE GSSetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
    {
        LLL;
        return m_pObject->GSSetShaderResources(
StartSlot,
NumViews,
ppShaderResourceViews
            );
    }

    virtual void STDMETHODCALLTYPE GSSetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
    {
        LLL;
        return m_pObject->GSSetSamplers(
StartSlot,
NumSamplers,
ppSamplers
            );
    }

    virtual void STDMETHODCALLTYPE OMSetRenderTargets(
        /* [annotation] */
        _In_range_( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11RenderTargetView *const *ppRenderTargetViews,
        /* [annotation] */
        _In_opt_  ID3D11DepthStencilView *pDepthStencilView)
    {
        LLL;
        return m_pObject->OMSetRenderTargets(
NumViews,
ppRenderTargetViews,
pDepthStencilView
            );
    }

    virtual void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(
        /* [annotation] */
        _In_  UINT NumRTVs,
        /* [annotation] */
        _In_reads_opt_(NumRTVs)  ID3D11RenderTargetView *const *ppRenderTargetViews,
        /* [annotation] */
        _In_opt_  ID3D11DepthStencilView *pDepthStencilView,
        /* [annotation] */
        _In_range_( 0, D3D11_1_UAV_SLOT_COUNT - 1 )  UINT UAVStartSlot,
        /* [annotation] */
        _In_  UINT NumUAVs,
        /* [annotation] */
        _In_reads_opt_(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
        /* [annotation] */
        _In_reads_opt_(NumUAVs)  const UINT *pUAVInitialCounts)
    {
        LLL;
        return m_pObject->OMSetRenderTargetsAndUnorderedAccessViews(
NumRTVs,
ppRenderTargetViews,
pDepthStencilView,
UAVStartSlot,
NumUAVs,
ppUnorderedAccessViews,
pUAVInitialCounts
            );
    }

    virtual void STDMETHODCALLTYPE OMSetBlendState(
        /* [annotation] */
        _In_opt_  ID3D11BlendState *pBlendState,
        /* [annotation] */
        _In_opt_  const FLOAT BlendFactor[ 4 ],
        /* [annotation] */
        _In_  UINT SampleMask)
    {
        LLL;
        return m_pObject->OMSetBlendState(
pBlendState,
BlendFactor,
SampleMask
            );
    }

    virtual void STDMETHODCALLTYPE OMSetDepthStencilState(
        /* [annotation] */
        _In_opt_  ID3D11DepthStencilState *pDepthStencilState,
        /* [annotation] */
        _In_  UINT StencilRef)
    {
        LLL;
        return m_pObject->OMSetDepthStencilState(
pDepthStencilState,
StencilRef
            );
    }

    virtual void STDMETHODCALLTYPE SOSetTargets(
        /* [annotation] */
        _In_range_( 0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  const UINT *pOffsets)
    {
        LLL;
        return m_pObject->SOSetTargets(
NumBuffers,
ppSOTargets,
pOffsets
            );
    }

    virtual void STDMETHODCALLTYPE DrawAuto( void)
    {
        LLL;
        return m_pObject->DrawAuto();
    }

    virtual void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(
        /* [annotation] */
        _In_  ID3D11Buffer *pBufferForArgs,
        /* [annotation] */
        _In_  UINT AlignedByteOffsetForArgs)
    {
        LLL;
        return m_pObject->DrawIndexedInstancedIndirect(
pBufferForArgs,
AlignedByteOffsetForArgs
            );
    }

    virtual void STDMETHODCALLTYPE DrawInstancedIndirect(
        /* [annotation] */
        _In_  ID3D11Buffer *pBufferForArgs,
        /* [annotation] */
        _In_  UINT AlignedByteOffsetForArgs)
    {
        LLL;
        return m_pObject->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
    }

    virtual void STDMETHODCALLTYPE Dispatch(
        /* [annotation] */
        _In_  UINT ThreadGroupCountX,
        /* [annotation] */
        _In_  UINT ThreadGroupCountY,
        /* [annotation] */
        _In_  UINT ThreadGroupCountZ)
    {
        LLL;
        return m_pObject->Dispatch(
ThreadGroupCountX,
ThreadGroupCountY,
ThreadGroupCountZ
            );
    }

    virtual void STDMETHODCALLTYPE DispatchIndirect(
        /* [annotation] */
        _In_  ID3D11Buffer *pBufferForArgs,
        /* [annotation] */
        _In_  UINT AlignedByteOffsetForArgs)
    {
        LLL;
        return m_pObject->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
    }

    virtual void STDMETHODCALLTYPE RSSetState(
        /* [annotation] */
        _In_opt_  ID3D11RasterizerState *pRasterizerState)
    {
        LLL;
        return m_pObject->RSSetState(pRasterizerState);
    }

    virtual void STDMETHODCALLTYPE RSSetViewports(
        /* [annotation] */
        _In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
        /* [annotation] */
        _In_reads_opt_(NumViewports)  const D3D11_VIEWPORT *pViewports)
    {
        LLL;
        return m_pObject->RSSetViewports(NumViewports, pViewports);
    }

    virtual void STDMETHODCALLTYPE RSSetScissorRects(
        /* [annotation] */
        _In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
        /* [annotation] */
        _In_reads_opt_(NumRects)  const D3D11_RECT *pRects)
    {
        LLL;
        return m_pObject->RSSetScissorRects(NumRects, pRects);
    }

    virtual void STDMETHODCALLTYPE CopySubresourceRegion(
        /* [annotation] */
        _In_  ID3D11Resource *pDstResource,
        /* [annotation] */
        _In_  UINT DstSubresource,
        /* [annotation] */
        _In_  UINT DstX,
        /* [annotation] */
        _In_  UINT DstY,
        /* [annotation] */
        _In_  UINT DstZ,
        /* [annotation] */
        _In_  ID3D11Resource *pSrcResource,
        /* [annotation] */
        _In_  UINT SrcSubresource,
        /* [annotation] */
        _In_opt_  const D3D11_BOX *pSrcBox)
    {
        LLL;
        return m_pObject->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
    }

    virtual void STDMETHODCALLTYPE CopyResource(
        /* [annotation] */
        _In_  ID3D11Resource *pDstResource,
        /* [annotation] */
        _In_  ID3D11Resource *pSrcResource)
    {
        LLL;
        return m_pObject->CopyResource(pDstResource, pSrcResource);
    }

    virtual void STDMETHODCALLTYPE UpdateSubresource(
        /* [annotation] */
        _In_  ID3D11Resource *pDstResource,
        /* [annotation] */
        _In_  UINT DstSubresource,
        /* [annotation] */
        _In_opt_  const D3D11_BOX *pDstBox,
        /* [annotation] */
        _In_  const void *pSrcData,
        /* [annotation] */
        _In_  UINT SrcRowPitch,
        /* [annotation] */
        _In_  UINT SrcDepthPitch)
    {
        LLL;
        return m_pObject->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
    }

    virtual void STDMETHODCALLTYPE CopyStructureCount(
        /* [annotation] */
        _In_  ID3D11Buffer *pDstBuffer,
        /* [annotation] */
        _In_  UINT DstAlignedByteOffset,
        /* [annotation] */
        _In_  ID3D11UnorderedAccessView *pSrcView)
    {
        LLL;
        return m_pObject->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
    }

    virtual void STDMETHODCALLTYPE ClearRenderTargetView(
        /* [annotation] */
        _In_  ID3D11RenderTargetView *pRenderTargetView,
        /* [annotation] */
        _In_  const FLOAT ColorRGBA[ 4 ])
    {
        LLL;
        return m_pObject->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
    }

    virtual void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
        /* [annotation] */
        _In_  ID3D11UnorderedAccessView *pUnorderedAccessView,
        /* [annotation] */
        _In_  const UINT Values[ 4 ])
    {
        LLL;
        return m_pObject->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
    }

    virtual void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
        /* [annotation] */
        _In_  ID3D11UnorderedAccessView *pUnorderedAccessView,
        /* [annotation] */
        _In_  const FLOAT Values[ 4 ])
    {
        LLL;
        return m_pObject->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
    }

    virtual void STDMETHODCALLTYPE ClearDepthStencilView(
        /* [annotation] */
        _In_  ID3D11DepthStencilView *pDepthStencilView,
        /* [annotation] */
        _In_  UINT ClearFlags,
        /* [annotation] */
        _In_  FLOAT Depth,
        /* [annotation] */
        _In_  UINT8 Stencil)
    {
        LLL;
        return m_pObject->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
    }

    virtual void STDMETHODCALLTYPE GenerateMips(
        /* [annotation] */
        _In_  ID3D11ShaderResourceView *pShaderResourceView)
    {
        LLL;
        return m_pObject->GenerateMips(pShaderResourceView);
    }

    virtual void STDMETHODCALLTYPE SetResourceMinLOD(
        /* [annotation] */
        _In_  ID3D11Resource *pResource,
        FLOAT MinLOD)
    {
        LLL;
        return m_pObject->SetResourceMinLOD(pResource, MinLOD);
    }

    virtual FLOAT STDMETHODCALLTYPE GetResourceMinLOD(
        /* [annotation] */
        _In_  ID3D11Resource *pResource)
    {
        LLL;
        return m_pObject->GetResourceMinLOD(pResource);
    }

    virtual void STDMETHODCALLTYPE ResolveSubresource(
        /* [annotation] */
        _In_  ID3D11Resource *pDstResource,
        /* [annotation] */
        _In_  UINT DstSubresource,
        /* [annotation] */
        _In_  ID3D11Resource *pSrcResource,
        /* [annotation] */
        _In_  UINT SrcSubresource,
        /* [annotation] */
        _In_  DXGI_FORMAT Format)
    {
        LLL;
        return m_pObject->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
    }

    virtual void STDMETHODCALLTYPE ExecuteCommandList(
        /* [annotation] */
        _In_  ID3D11CommandList *pCommandList,
        BOOL RestoreContextState)
    {
        LLL;
        return m_pObject->ExecuteCommandList(pCommandList, RestoreContextState);
    }

    virtual void STDMETHODCALLTYPE HSSetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
    {
        LLL;
        return m_pObject->HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    virtual void STDMETHODCALLTYPE HSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11HullShader *pHullShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances)
    {
        LLL;
        return m_pObject->HSSetShader(pHullShader, ppClassInstances, NumClassInstances);
    }

    virtual void STDMETHODCALLTYPE HSSetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
    {
        LLL;
        return m_pObject->HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
    }

    virtual void STDMETHODCALLTYPE HSSetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
    {
        LLL;
        return m_pObject->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }

    virtual void STDMETHODCALLTYPE DSSetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
    {
        LLL;
        return m_pObject->DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    virtual void STDMETHODCALLTYPE DSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11DomainShader *pDomainShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances)
    {
        LLL;
        return m_pObject->DSSetShader(pDomainShader, ppClassInstances, NumClassInstances);
    }

    virtual void STDMETHODCALLTYPE DSSetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
    {
        LLL;
        return m_pObject->DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
    }

    virtual void STDMETHODCALLTYPE DSSetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
    {
        LLL;
        return m_pObject->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }

    virtual void STDMETHODCALLTYPE CSSetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
    {
        LLL;
        return m_pObject->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    virtual void STDMETHODCALLTYPE CSSetUnorderedAccessViews(
        /* [annotation] */
        _In_range_( 0, D3D11_1_UAV_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_1_UAV_SLOT_COUNT - StartSlot )  UINT NumUAVs,
        /* [annotation] */
        _In_reads_opt_(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
        /* [annotation] */
        _In_reads_opt_(NumUAVs)  const UINT *pUAVInitialCounts)
    {
        LLL;
        return m_pObject->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
    }

    virtual void STDMETHODCALLTYPE CSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11ComputeShader *pComputeShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances)
    {
        LLL;
        return m_pObject->CSSetShader(pComputeShader, ppClassInstances, NumClassInstances);
    }

    virtual void STDMETHODCALLTYPE CSSetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
    {
        LLL;
        return m_pObject->CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
    }

    virtual void STDMETHODCALLTYPE CSSetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
    {
        LLL;
        return m_pObject->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }

    virtual void STDMETHODCALLTYPE VSGetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
    {
        LLL;
        return m_pObject->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }

    virtual void STDMETHODCALLTYPE PSGetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
    {
        LLL;
        return m_pObject->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    virtual void STDMETHODCALLTYPE PSGetShader(
        /* [annotation] */
        _Out_  ID3D11PixelShader **ppPixelShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances)
    {
        LLL;
        return m_pObject->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances);
    }

    virtual void STDMETHODCALLTYPE PSGetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers)
    {
        LLL;
        return m_pObject->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }

    virtual void STDMETHODCALLTYPE VSGetShader(
        /* [annotation] */
        _Out_  ID3D11VertexShader **ppVertexShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances)
    {
        LLL;
        return m_pObject->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);
    }

    virtual void STDMETHODCALLTYPE PSGetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
    {
        LLL;
        return m_pObject->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }

    virtual void STDMETHODCALLTYPE IAGetInputLayout(
        /* [annotation] */
        _Out_  ID3D11InputLayout **ppInputLayout)
    {
        LLL;
        return m_pObject->IAGetInputLayout(ppInputLayout);
    }

    virtual void STDMETHODCALLTYPE IAGetVertexBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppVertexBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  UINT *pStrides,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  UINT *pOffsets)
    {
        LLL;
        return m_pObject->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
    }

    virtual void STDMETHODCALLTYPE IAGetIndexBuffer(
        /* [annotation] */
        _Out_opt_  ID3D11Buffer **pIndexBuffer,
        /* [annotation] */
        _Out_opt_  DXGI_FORMAT *Format,
        /* [annotation] */
        _Out_opt_  UINT *Offset)
    {
        LLL;
        return m_pObject->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
    }

    virtual void STDMETHODCALLTYPE GSGetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
    {
        LLL;
        return m_pObject->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }

    virtual void STDMETHODCALLTYPE GSGetShader(
        /* [annotation] */
        _Out_  ID3D11GeometryShader **ppGeometryShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances)
    {
        LLL;
        return m_pObject->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances);
    }

    virtual void STDMETHODCALLTYPE IAGetPrimitiveTopology(
        /* [annotation] */
        _Out_  D3D11_PRIMITIVE_TOPOLOGY *pTopology)
    {
        LLL;
        return m_pObject->IAGetPrimitiveTopology(pTopology);
    }

    virtual void STDMETHODCALLTYPE VSGetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
    {
        LLL;
        return m_pObject->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    virtual void STDMETHODCALLTYPE VSGetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers)
    {
        LLL;
        return m_pObject->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }

    virtual void STDMETHODCALLTYPE GetPredication(
        /* [annotation] */
        _Out_opt_  ID3D11Predicate **ppPredicate,
        /* [annotation] */
        _Out_opt_  BOOL *pPredicateValue)
    {
        LLL;
        return m_pObject->GetPredication(ppPredicate, pPredicateValue);
    }

    virtual void STDMETHODCALLTYPE GSGetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
    {
        LLL;
        return m_pObject->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    virtual void STDMETHODCALLTYPE GSGetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers)
    {
        LLL;
        return m_pObject->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }

    virtual void STDMETHODCALLTYPE OMGetRenderTargets(
        /* [annotation] */
        _In_range_( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
        /* [annotation] */
        _Out_opt_  ID3D11DepthStencilView **ppDepthStencilView)
    {
        LLL;
        return m_pObject->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
    }

    virtual void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(
        /* [annotation] */
        _In_range_( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumRTVs,
        /* [annotation] */
        _Out_writes_opt_(NumRTVs)  ID3D11RenderTargetView **ppRenderTargetViews,
        /* [annotation] */
        _Out_opt_  ID3D11DepthStencilView **ppDepthStencilView,
        /* [annotation] */
        _In_range_( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT UAVStartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot )  UINT NumUAVs,
        /* [annotation] */
        _Out_writes_opt_(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
    {
        LLL;
        return m_pObject->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView,
                                                            UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
    }

    virtual void STDMETHODCALLTYPE OMGetBlendState(
        /* [annotation] */
        _Out_opt_  ID3D11BlendState **ppBlendState,
        /* [annotation] */
        _Out_opt_  FLOAT BlendFactor[ 4 ],
        /* [annotation] */
        _Out_opt_  UINT *pSampleMask)
    {
        LLL;
        return m_pObject->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
    }

    virtual void STDMETHODCALLTYPE OMGetDepthStencilState(
        /* [annotation] */
        _Out_opt_  ID3D11DepthStencilState **ppDepthStencilState,
        /* [annotation] */
        _Out_opt_  UINT *pStencilRef)
    {
        LLL;
        return m_pObject->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
    }

    virtual void STDMETHODCALLTYPE SOGetTargets(
        /* [annotation] */
        _In_range_( 0, D3D11_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppSOTargets)
    {
        LLL;
        return m_pObject->SOGetTargets(NumBuffers, ppSOTargets);
    }

    virtual void STDMETHODCALLTYPE RSGetState(
        /* [annotation] */
        _Out_  ID3D11RasterizerState **ppRasterizerState)
    {
        LLL;
        return m_pObject->RSGetState(ppRasterizerState );
    }

    virtual void STDMETHODCALLTYPE RSGetViewports(
        /* [annotation] */
        _Inout_ /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
        /* [annotation] */
        _Out_writes_opt_(*pNumViewports)  D3D11_VIEWPORT *pViewports)
    {
        LLL;
        return m_pObject->RSGetViewports(pNumViewports, pViewports);
    }

    virtual void STDMETHODCALLTYPE RSGetScissorRects(
        /* [annotation] */
        _Inout_ /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
        /* [annotation] */
        _Out_writes_opt_(*pNumRects)  D3D11_RECT *pRects)
    {
        LLL;
        return m_pObject->RSGetScissorRects(pNumRects, pRects);
    }

    virtual void STDMETHODCALLTYPE HSGetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
    {
        LLL;
        return m_pObject->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    virtual void STDMETHODCALLTYPE HSGetShader(
        /* [annotation] */
        _Out_  ID3D11HullShader **ppHullShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances)
    {
        LLL;
        return m_pObject->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances);
    }

    virtual void STDMETHODCALLTYPE HSGetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers)
    {
        LLL;
        return m_pObject->HSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }

    virtual void STDMETHODCALLTYPE HSGetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
    {
        LLL;
        return m_pObject->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }

    virtual void STDMETHODCALLTYPE DSGetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
    {
        LLL;
        return m_pObject->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    virtual void STDMETHODCALLTYPE DSGetShader(
        /* [annotation] */
        _Out_  ID3D11DomainShader **ppDomainShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances)
    {
        LLL;
        return m_pObject->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances);
    }

    virtual void STDMETHODCALLTYPE DSGetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers)
    {
        LLL;
        return m_pObject->DSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }

    virtual void STDMETHODCALLTYPE DSGetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
    {
        LLL;
        return m_pObject->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }

    virtual void STDMETHODCALLTYPE CSGetShaderResources(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
    {
        LLL;
        return m_pObject->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    virtual void STDMETHODCALLTYPE CSGetUnorderedAccessViews(
        /* [annotation] */
        _In_range_( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
        /* [annotation] */
        _Out_writes_opt_(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
    {
        LLL;
        return m_pObject->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews);
    }

    virtual void STDMETHODCALLTYPE CSGetShader(
        /* [annotation] */
        _Out_  ID3D11ComputeShader **ppComputeShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances)
    {
        LLL;
        return m_pObject->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances);
    }

    virtual void STDMETHODCALLTYPE CSGetSamplers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers)
    {
        LLL;
        return m_pObject->CSGetSamplers(StartSlot, NumSamplers, ppSamplers);
    }

    virtual void STDMETHODCALLTYPE CSGetConstantBuffers(
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
    {
        LLL;
        return m_pObject->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    }

    virtual void STDMETHODCALLTYPE ClearState( void)
    {
        LLL;
        return m_pObject->ClearState();
    }

    virtual void STDMETHODCALLTYPE Flush( void)
    {
        LLL;
        return m_pObject->Flush();
    }

    virtual D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType()
    {
        LLL;
        return m_pObject->GetType();
    }

    virtual UINT STDMETHODCALLTYPE GetContextFlags()
    {
        LLL;
        return m_pObject->GetContextFlags();
    }

    virtual HRESULT STDMETHODCALLTYPE FinishCommandList(
        BOOL RestoreDeferredContextState,
        /* [annotation] */
        _Out_opt_  ID3D11CommandList **ppCommandList)
    {
        LLL;
        return m_pObject->FinishCommandList(RestoreDeferredContextState, ppCommandList);
    }

};


class SpyID3D11Device
    : public ID3D11Device
{
    ID3D11Device *m_pOriginal;
public :
    SpyID3D11Device(ID3D11Device *pOrigDevice)
        : m_pOriginal(pOrigDevice)
        , m_nRefCount(1)
    {
        LLL;
    }
    virtual ~SpyID3D11Device()
    {
    }
    INKNOWN_IMPL_REF(m_pOriginal);

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **pp)
    {
        HRESULT hr  = S_OK;

        LLL;


        if (id == __uuidof(ID3D11VideoDevice))
        {
            logs("ID3D11VideoDevice");
            ID3D11VideoDevice *pVideoDevice;

            hr = m_pOriginal->QueryInterface(id, (void**)&pVideoDevice);

            if (hr == S_OK)
            {
                *pp = new SpyD3D11VideoDevice(pVideoDevice);
            }
        }
        else if (id == __uuidof(ID3D10Multithread))
        {
            logs("ID3D10Multithread");
            ID3D10Multithread *pThread;
            hr = m_pOriginal->QueryInterface(id, (void**)&pThread);
            if (S_OK == hr)
            {
                *pp = new SpyD3D10Multithreaded(pThread);
            }
        }
        else if (id == __uuidof(ID3D11Device))
        {
            logs("ID3D11Device");
            //should return this
            AddRef();
            *pp = (ID3D11Device*)this;
        }
        else
        {
            LOG_GUID(id);
            hr = m_pOriginal->QueryInterface(id, pp);
        }

        logi(hr);
        return hr;
    }

    virtual HRESULT STDMETHODCALLTYPE CreateBuffer(
        const D3D11_BUFFER_DESC *pDesc,
        const D3D11_SUBRESOURCE_DATA *pInitialData,
        ID3D11Buffer **ppBuffer)
    {
        LLL;
        return m_pOriginal->CreateBuffer(pDesc, pInitialData, ppBuffer);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateTexture1D(
        const D3D11_TEXTURE1D_DESC *pDesc,
        const D3D11_SUBRESOURCE_DATA *pInitialData,
        ID3D11Texture1D **ppTexture1D)
    {
        LLL;
        return m_pOriginal->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateTexture2D(
        const D3D11_TEXTURE2D_DESC *pDesc,
        const D3D11_SUBRESOURCE_DATA *pInitialData,
          ID3D11Texture2D **ppTexture2D)
    {
        LLL;
        return m_pOriginal->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateTexture3D(
          const D3D11_TEXTURE3D_DESC *pDesc,
          const D3D11_SUBRESOURCE_DATA *pInitialData,
          ID3D11Texture3D **ppTexture3D)
    {
        LLL;
        return m_pOriginal->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
          ID3D11Resource *pResource,
          const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
          ID3D11ShaderResourceView **ppSRView)
    {
        LLL;
        return m_pOriginal->CreateShaderResourceView(pResource, pDesc, ppSRView);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
          ID3D11Resource *pResource,
          const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
          ID3D11UnorderedAccessView **ppUAView)
    {
        LLL;
        return m_pOriginal->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetView(

          ID3D11Resource *pResource,

          const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,

          ID3D11RenderTargetView **ppRTView)
    {
        LLL;
        return m_pOriginal->CreateRenderTargetView(pResource, pDesc, ppRTView);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
          ID3D11Resource *pResource,
          const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
          ID3D11DepthStencilView **ppDepthStencilView)
    {
        return m_pOriginal->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateInputLayout(
          const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
          UINT NumElements,
          const void *pShaderBytecodeWithInputSignature,
          SIZE_T BytecodeLength,
          ID3D11InputLayout **ppInputLayout)
    {
        LLL;
        return m_pOriginal->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateVertexShader(
          const void *pShaderBytecode,
          SIZE_T BytecodeLength,
          ID3D11ClassLinkage *pClassLinkage,
          ID3D11VertexShader **ppVertexShader)
    {
        LLL;
        return m_pOriginal->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateGeometryShader(
          const void *pShaderBytecode,
          SIZE_T BytecodeLength,
          ID3D11ClassLinkage *pClassLinkage,
          ID3D11GeometryShader **ppGeometryShader)
    {
        LLL;
        return m_pOriginal->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
          const void *pShaderBytecode,
          SIZE_T BytecodeLength,
          const D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
          UINT NumEntries,
          const UINT *pBufferStrides,
          UINT NumStrides,
          UINT RasterizedStream,
          ID3D11ClassLinkage *pClassLinkage,
          ID3D11GeometryShader **ppGeometryShader)
    {
        LLL;
        return m_pOriginal->CreateGeometryShaderWithStreamOutput(
            pShaderBytecode,
            BytecodeLength,
            pSODeclaration,
            NumEntries,
            pBufferStrides,
            NumStrides,
            RasterizedStream,
            pClassLinkage,
            ppGeometryShader);
    }

    virtual HRESULT STDMETHODCALLTYPE CreatePixelShader(
          const void *pShaderBytecode,
          SIZE_T BytecodeLength,
          ID3D11ClassLinkage *pClassLinkage,
          ID3D11PixelShader **ppPixelShader)
    {
        LLL;
        return m_pOriginal->CreatePixelShader( pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateHullShader(
          const void *pShaderBytecode,
          SIZE_T BytecodeLength,
          ID3D11ClassLinkage *pClassLinkage,
          ID3D11HullShader **ppHullShader)
    {
        LLL;
        return m_pOriginal->CreateHullShader( pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateDomainShader(
          const void *pShaderBytecode,
          SIZE_T BytecodeLength,
          ID3D11ClassLinkage *pClassLinkage,
          ID3D11DomainShader **ppDomainShader)
    {
        LLL;
        return m_pOriginal->CreateDomainShader( pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateComputeShader(
          const void *pShaderBytecode,
          SIZE_T BytecodeLength,
          ID3D11ClassLinkage *pClassLinkage,
          ID3D11ComputeShader **ppComputeShader)
    {
        LLL;
        return m_pOriginal->CreateComputeShader( pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateClassLinkage(
          ID3D11ClassLinkage **ppLinkage)
    {
        LLL;
        return m_pOriginal->CreateClassLinkage( ppLinkage);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateBlendState(
          const D3D11_BLEND_DESC *pBlendStateDesc,
          ID3D11BlendState **ppBlendState)
    {
        LLL;
        return m_pOriginal->CreateBlendState( pBlendStateDesc, ppBlendState);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
          const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
          ID3D11DepthStencilState **ppDepthStencilState)
    {
        LLL;
        return m_pOriginal->CreateDepthStencilState( pDepthStencilDesc, ppDepthStencilState);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateRasterizerState(
        const D3D11_RASTERIZER_DESC *pRasterizerDesc,
        ID3D11RasterizerState **ppRasterizerState)
    {
        LLL;
        return m_pOriginal->CreateRasterizerState( pRasterizerDesc, ppRasterizerState);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateSamplerState(
          const D3D11_SAMPLER_DESC *pSamplerDesc,
          ID3D11SamplerState **ppSamplerState)
    {
        LLL;
        return m_pOriginal->CreateSamplerState( pSamplerDesc, ppSamplerState);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateQuery(
          const D3D11_QUERY_DESC *pQueryDesc,
          ID3D11Query **ppQuery)
    {

        LLL;
        return m_pOriginal->CreateQuery( pQueryDesc, ppQuery);
    }

    virtual HRESULT STDMETHODCALLTYPE CreatePredicate(
          const D3D11_QUERY_DESC *pPredicateDesc,
          ID3D11Predicate **ppPredicate)
    {
        LLL;
        return m_pOriginal->CreatePredicate( pPredicateDesc, ppPredicate);
    }
    virtual HRESULT STDMETHODCALLTYPE CreateCounter(
          const D3D11_COUNTER_DESC *pCounterDesc,
          ID3D11Counter **ppCounter)
    {
        LLL;
        return m_pOriginal->CreateCounter( pCounterDesc, ppCounter);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateDeferredContext(
          UINT ContextFlags,
          ID3D11DeviceContext **ppDeferredContext)
    {
        LLL;
        return m_pOriginal->CreateDeferredContext( ContextFlags, ppDeferredContext);
    }

    virtual HRESULT STDMETHODCALLTYPE OpenSharedResource(
          HANDLE hResource,
          REFIID ReturnedInterface,
          void **ppResource)
    {
        LLL;
        return m_pOriginal->OpenSharedResource( hResource, ReturnedInterface, ppResource);
    }

    virtual HRESULT STDMETHODCALLTYPE CheckFormatSupport(
          DXGI_FORMAT Format,
          UINT *pFormatSupport)
    {
        LLL;
        return m_pOriginal->CheckFormatSupport( Format, pFormatSupport);
    }

    virtual HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
          DXGI_FORMAT Format,
          UINT SampleCount,
          UINT *pNumQualityLevels)
    {
        LLL;
        return m_pOriginal->CheckMultisampleQualityLevels( Format, SampleCount, pNumQualityLevels);
    }

    virtual void STDMETHODCALLTYPE CheckCounterInfo(
          D3D11_COUNTER_INFO *pCounterInfo)
    {
        LLL;
        return m_pOriginal->CheckCounterInfo(pCounterInfo);
    }

    virtual HRESULT STDMETHODCALLTYPE CheckCounter(
        const D3D11_COUNTER_DESC *pDesc,
        D3D11_COUNTER_TYPE *pType,
        UINT *pActiveCounters,
        LPSTR szName,
        UINT *pNameLength,
        LPSTR szUnits,
        UINT *pUnitsLength,
        LPSTR szDescription,
        UINT *pDescriptionLength)
    {
        LLL;
        return m_pOriginal->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
    }

    virtual HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
        D3D11_FEATURE Feature,
        void *pFeatureSupportData,
        UINT FeatureSupportDataSize)
    {
        LLL;
        return m_pOriginal->CheckFeatureSupport(Feature, pFeatureSupportData,FeatureSupportDataSize);
    }

    virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
        REFGUID guid,
        UINT *pDataSize,
        void *pData)
    {
        LLL;
        return m_pOriginal->GetPrivateData(guid, pDataSize, pData);
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
          REFGUID guid,
          UINT DataSize,
          const void *pData)
    {
        LLL;
        return m_pOriginal->SetPrivateData(guid, DataSize, pData);
    }

    virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
          REFGUID guid,
          const IUnknown *pData)
    {
        LLL;
        return m_pOriginal->SetPrivateDataInterface(guid, pData);
    }

    virtual D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel( void)
    {
        LLL;
        return m_pOriginal->GetFeatureLevel();
    }

    virtual UINT STDMETHODCALLTYPE GetCreationFlags( void)
    {
        LLL;
        return m_pOriginal->GetCreationFlags();
    }

    virtual HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason( void)
    {
        LLL;
        return m_pOriginal->GetDeviceRemovedReason();
    }

    virtual void STDMETHODCALLTYPE GetImmediateContext(
          ID3D11DeviceContext **ppImmediateContext)
    {
        LLL;
        m_pOriginal->GetImmediateContext(ppImmediateContext);
        if (ppImmediateContext && *ppImmediateContext)
        {
            *ppImmediateContext = new SpyD3D11DeviceContext(*ppImmediateContext);
        }
    }

    virtual HRESULT STDMETHODCALLTYPE SetExceptionMode(
        UINT RaiseFlags)
    {
        LLL;
        return m_pOriginal->SetExceptionMode(RaiseFlags);
    }

    virtual UINT STDMETHODCALLTYPE GetExceptionMode( void)
    {
        LLL;
        return m_pOriginal->GetExceptionMode();
    }
};
