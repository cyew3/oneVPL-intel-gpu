/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2012 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __DXVA2_SPY_H
#define __DXVA2_SPY_H

struct IDirect3DDeviceManager9;
struct IDirectXVideoDecoderService;
struct IDirectXVideoDecoder;
struct ID3D11Device;

void SetDumpingDirectory(char *dir);
void SkipDXVAExecute(bool bSkipExecute);

IDirect3DDeviceManager9     *CreateDXVASpy(IDirect3DDeviceManager9*);
IDirectXVideoDecoderService *CreateDXVASpy(IDirectXVideoDecoderService *pObject);
IDirectXVideoDecoder        *CreateDXVASpy(IDirectXVideoDecoder *pObject);
ID3D11Device                *CreateDXVASpy(ID3D11Device* pObject);

#ifndef DONT_USE_DXVA2API

#include <dshow.h>
#include <dvdmedia.h>
#include <sal.h>
#include <rpcsal.h>
#include <d3d9.h>
#include <dxva.h>
#include <dxva2api.h>


namespace UMC
{
    enum VideoAccelerationProfile;
};

///////////////////////////////////////////////////////////////////////

#define INKNOWN_IMPL_REF(pObject) \
    LONG m_nRefCount; \
    STDMETHODIMP_(ULONG) AddRef() \
{ \
    return InterlockedIncrement(&m_nRefCount); \
} \
    STDMETHODIMP_(ULONG) Release() \
{ \
    ULONG uCount = InterlockedDecrement(&m_nRefCount); \
    if (uCount == 0) { pObject->Release(); pObject = NULL; delete this; } \
    return uCount; \
}

#define INKNOWN_IMPL(pObject) \
    STDMETHODIMP QueryInterface(REFIID id, void **pp) \
{ \
    return pObject->QueryInterface(id, pp); \
} \
    INKNOWN_IMPL_REF(pObject)

#define INKNOWN_IMPL2(pObject) \
    STDMETHODIMP QueryInterface(REFIID id, void **pp); \
    INKNOWN_IMPL_REF(pObject)

////////////////////////////////////

class CSpyDirect3DDeviceManager9 : public IDirect3DDeviceManager9
{
public:
    CSpyDirect3DDeviceManager9(IDirect3DDeviceManager9 *pObject); 

    virtual HRESULT STDMETHODCALLTYPE ResetDevice(
        /* [in] */
        __in  IDirect3DDevice9 *pDevice,
        /* [in] */
        __in  UINT resetToken); 

    virtual HRESULT STDMETHODCALLTYPE OpenDeviceHandle(
        /* [out] */
        __out  HANDLE *phDevice); 

    virtual HRESULT STDMETHODCALLTYPE CloseDeviceHandle(
        /* [in] */
        __in  HANDLE hDevice); 

    virtual HRESULT STDMETHODCALLTYPE TestDevice(
        /* [in] */
        __in  HANDLE hDevice); 

    virtual HRESULT STDMETHODCALLTYPE LockDevice(
        /* [in] */
        __in  HANDLE hDevice,
        /* [out] */
        __deref_out  IDirect3DDevice9 **ppDevice,
        /* [in] */
        __in  BOOL fBlock); 

    virtual HRESULT STDMETHODCALLTYPE UnlockDevice(
        /* [in] */
        __in  HANDLE hDevice,
        /* [in] */
        __in  BOOL fSaveState); 

    virtual HRESULT STDMETHODCALLTYPE GetVideoService(
        /* [in] */
        __in  HANDLE hDevice,
        /* [in] */
        __in  REFIID riid,
        /* [out] */
        __deref_out  void **ppService);

      INKNOWN_IMPL2(m_pObject);

protected:
    IDirect3DDeviceManager9 *m_pObject;
};

////////////////////////////////////

class CSpyVideoDecoderService : public IDirectXVideoDecoderService
{
public:
    CSpyVideoDecoderService(IDirectXVideoDecoderService *pObject) :
      m_pObject(pObject)
      , m_nRefCount(0)
      {
      };

      virtual HRESULT STDMETHODCALLTYPE GetDecoderDeviceGuids(
          /* [out] */
          __out  UINT *pCount,
          /* [size_is][unique][out] */
          __deref_out_ecount_opt(*pCount)  GUID **pGuids);

      virtual HRESULT STDMETHODCALLTYPE GetDecoderRenderTargets(
          /* [in] */
          __in  REFGUID Guid,
          /* [out] */
          __out  UINT *pCount,
          /* [size_is][unique][out] */
          __deref_out_ecount_opt(*pCount)  D3DFORMAT **pFormats);

      virtual HRESULT STDMETHODCALLTYPE GetDecoderConfigurations(
          /* [in] */
          __in  REFGUID Guid,
          /* [in] */
          __in  const DXVA2_VideoDesc *pVideoDesc,
          /* [in] */
          __reserved  void *pReserved,
          /* [out] */
          __out  UINT *pCount,
          /* [size_is][unique][out] */
          __deref_out_ecount_opt(*pCount)  DXVA2_ConfigPictureDecode **ppConfigs);

      virtual HRESULT STDMETHODCALLTYPE CreateVideoDecoder(
          /* [in] */
          __in  REFGUID Guid,
          /* [in] */
          __in  const DXVA2_VideoDesc *pVideoDesc,
          /* [in] */
          __in  const DXVA2_ConfigPictureDecode *pConfig,
          /* [size_is][in] */
          __in_ecount(NumRenderTargets)  IDirect3DSurface9 **ppDecoderRenderTargets,
          /* [in] */
          __in  UINT NumRenderTargets,
          /* [out] */
          __deref_out  IDirectXVideoDecoder **ppDecode);

      //IDirectXVideoAccelerationService
      virtual HRESULT STDMETHODCALLTYPE CreateSurface(
          /* [in] */
          __in  UINT Width,
          /* [in] */
          __in  UINT Height,
          /* [in] */
          __in  UINT BackBuffers,
          /* [in] */
          __in  D3DFORMAT Format,
          /* [in] */
          __in  D3DPOOL Pool,
          /* [in] */
          __in  DWORD Usage,
          /* [in] */
          __in  DWORD DxvaType,
          /* [size_is][out] */
          __out_ecount(BackBuffers+1)  IDirect3DSurface9 **ppSurface,
          /* [out][in] */
          __inout_opt  HANDLE *pSharedHandle);

      INKNOWN_IMPL(m_pObject);

protected:
    IDirectXVideoDecoderService *m_pObject;
};

// 
class CSpyVideoProcessorService : public IDirectXVideoProcessorService
{
public:

    CSpyVideoProcessorService(IDirectXVideoProcessorService *pService);

    virtual HRESULT STDMETHODCALLTYPE RegisterVideoProcessorSoftwareDevice( 
        /* [in] */ 
        __in  void *pCallbacks) ;
    
    virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorDeviceGuids( 
        /* [in] */ 
        __in  const DXVA2_VideoDesc *pVideoDesc,
        /* [out] */ 
        __out  UINT *pCount,
        /* [size_is][unique][out] */ 
        __deref_out_ecount_opt(*pCount)  GUID **pGuids) ;
    
    virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorRenderTargets( 
        /* [in] */ 
        __in  REFGUID VideoProcDeviceGuid,
        /* [in] */ 
        __in  const DXVA2_VideoDesc *pVideoDesc,
        /* [out] */ 
        __out  UINT *pCount,
        /* [size_is][unique][out] */ 
        __deref_out_ecount_opt(*pCount)  D3DFORMAT **pFormats) ;
    
    virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorSubStreamFormats( 
        /* [in] */ 
        __in  REFGUID VideoProcDeviceGuid,
        /* [in] */ 
        __in  const DXVA2_VideoDesc *pVideoDesc,
        /* [in] */ 
        __in  D3DFORMAT RenderTargetFormat,
        /* [out] */ 
        __out  UINT *pCount,
        /* [size_is][unique][out] */ 
        __deref_out_ecount_opt(*pCount)  D3DFORMAT **pFormats) ;
    
    virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorCaps( 
        /* [in] */ 
        __in  REFGUID VideoProcDeviceGuid,
        /* [in] */ 
        __in  const DXVA2_VideoDesc *pVideoDesc,
        /* [in] */ 
        __in  D3DFORMAT RenderTargetFormat,
        /* [out] */ 
        __out  DXVA2_VideoProcessorCaps *pCaps) ;
    
    virtual HRESULT STDMETHODCALLTYPE GetProcAmpRange( 
        /* [in] */ 
        __in  REFGUID VideoProcDeviceGuid,
        /* [in] */ 
        __in  const DXVA2_VideoDesc *pVideoDesc,
        /* [in] */ 
        __in  D3DFORMAT RenderTargetFormat,
        /* [in] */ 
        __in  UINT ProcAmpCap,
        /* [out] */ 
        __out  DXVA2_ValueRange *pRange) ;
    
    virtual HRESULT STDMETHODCALLTYPE GetFilterPropertyRange( 
        /* [in] */ 
        __in  REFGUID VideoProcDeviceGuid,
        /* [in] */ 
        __in  const DXVA2_VideoDesc *pVideoDesc,
        /* [in] */ 
        __in  D3DFORMAT RenderTargetFormat,
        /* [in] */ 
        __in  UINT FilterSetting,
        /* [out] */ 
        __out  DXVA2_ValueRange *pRange) ;
    
    virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessor( 
        /* [in] */ 
        __in  REFGUID VideoProcDeviceGuid,
        /* [in] */ 
        __in  const DXVA2_VideoDesc *pVideoDesc,
        /* [in] */ 
        __in  D3DFORMAT RenderTargetFormat,
        /* [in] */ 
        __in  UINT MaxNumSubStreams,
        /* [out] */ 
        __deref_out  IDirectXVideoProcessor **ppVidProcess);

      //IDirectXVideoAccelerationService
      virtual HRESULT STDMETHODCALLTYPE CreateSurface(
          /* [in] */
          __in  UINT Width,
          /* [in] */
          __in  UINT Height,
          /* [in] */
          __in  UINT BackBuffers,
          /* [in] */
          __in  D3DFORMAT Format,
          /* [in] */
          __in  D3DPOOL Pool,
          /* [in] */
          __in  DWORD Usage,
          /* [in] */
          __in  DWORD DxvaType,
          /* [size_is][out] */
          __out_ecount(BackBuffers+1)  IDirect3DSurface9 **ppSurface,
          /* [out][in] */
          __inout_opt  HANDLE *pSharedHandle);
    
    INKNOWN_IMPL(m_pObject);

protected:
    IDirectXVideoProcessorService *m_pObject;
};

class CSpyVideoProcessor : public IDirectXVideoProcessor
{
public:
        CSpyVideoProcessor(IDirectXVideoProcessor *pObject); 

        virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorService( 
            /* [out] */ 
            __deref_out  IDirectXVideoProcessorService **ppService) ;
        
        virtual HRESULT STDMETHODCALLTYPE GetCreationParameters( 
            /* [out] */ 
            __out_opt  GUID *pDeviceGuid,
            /* [out] */ 
            __out_opt  DXVA2_VideoDesc *pVideoDesc,
            /* [out] */ 
            __out_opt  D3DFORMAT *pRenderTargetFormat,
            /* [out] */ 
            __out_opt  UINT *pMaxNumSubStreams) ;
        
        virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorCaps( 
            /* [out] */ 
            __out  DXVA2_VideoProcessorCaps *pCaps) ;
        
        virtual HRESULT STDMETHODCALLTYPE GetProcAmpRange( 
            /* [in] */ 
            __in  UINT ProcAmpCap,
            /* [out] */ 
            __out  DXVA2_ValueRange *pRange) ;
        
        virtual HRESULT STDMETHODCALLTYPE GetFilterPropertyRange( 
            /* [in] */ 
            __in  UINT FilterSetting,
            /* [out] */ 
            __out  DXVA2_ValueRange *pRange) ;
        
        virtual HRESULT STDMETHODCALLTYPE VideoProcessBlt( 
            /* [in] */ 
            __in  IDirect3DSurface9 *pRenderTarget,
            /* [in] */ 
            __in  const DXVA2_VideoProcessBltParams *pBltParams,
            /* [size_is][in] */ 
            __in_ecount(NumSamples)  const DXVA2_VideoSample *pSamples,
            /* [in] */ 
            __in  UINT NumSamples,
            /* [out] */ 
            __inout_opt  HANDLE *pHandleComplete) ;
        
        INKNOWN_IMPL(m_pObject);
        
protected:
    IDirectXVideoProcessor * m_pObject;
};
////////////////////////////////////

class CSpyVideoDecoder : public IDirectXVideoDecoder
{
public:
    CSpyVideoDecoder(IDirectXVideoDecoder *pObject) :
      m_pObject(pObject)
      , m_nRefCount(0)
      {
      };

      virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderService(
          /* [out] */
          __deref_out  IDirectXVideoDecoderService **ppService);

      virtual HRESULT STDMETHODCALLTYPE GetCreationParameters(
          /* [out] */
          __out_opt  GUID *pDeviceGuid,
          /* [out] */
          __out_opt  DXVA2_VideoDesc *pVideoDesc,
          /* [out] */
          __out_opt  DXVA2_ConfigPictureDecode *pConfig,
          /* [size_is][unique][out] */
          __out_ecount(*pNumSurfaces)  IDirect3DSurface9 ***pDecoderRenderTargets,
          /* [out] */
          __out_opt  UINT *pNumSurfaces);

      virtual HRESULT STDMETHODCALLTYPE GetBuffer(
          /* [in] */
          __in  UINT BufferType,
          /* [out] */
          __out  void **ppBuffer,
          /* [out] */
          __out  UINT *pBufferSize);

      virtual HRESULT STDMETHODCALLTYPE ReleaseBuffer(
          /* [in] */
          __in  UINT BufferType);

      virtual HRESULT STDMETHODCALLTYPE BeginFrame(
          /* [in] */
          __in  IDirect3DSurface9 *pRenderTarget,
          /* [in] */
          __in_opt  void *pvPVPData);

      virtual HRESULT STDMETHODCALLTYPE EndFrame(
          /* [out] */
          __inout_opt  HANDLE *pHandleComplete);

      virtual HRESULT STDMETHODCALLTYPE Execute(
          /* [in] */
          __in  const DXVA2_DecodeExecuteParams *pExecuteParams);

      INKNOWN_IMPL(m_pObject);

protected:
    IDirectXVideoDecoder *m_pObject;
};

#endif //#ifndef DONT_USE_DXVA2API
#endif //#ifndef __DXVA2_SPY_H