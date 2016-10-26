//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2008 Intel Corporation. All Rights Reserved.
//

// This is a copy of some MediaFoundation DXVA2 declarations

#pragma once

#include <wtypes.h>
#include <sal.h>
#include <rpcsal.h>
//#include <mfapi.h>

static const GUID MR_VIDEO_ACCELERATION_SERVICE =
{
    0xefef5175,
    0x5c7d,
    0x4ce2,
    0xbb, 0xbd, 0x34, 0xff, 0x8b, 0xca, 0x65, 0x54
};

static const GUID MR_VIDEO_RENDER_SERVICE =
{
    0x1092a86c,
    0xab1a,
    0x459a,
    0xa3, 0x36, 0x83, 0x1f, 0xbc, 0x4d, 0x11, 0xff
};

MIDL_INTERFACE("fa993888-4383-415a-a930-dd472a8cf6f7")
IMFGetService : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetService(
        /* [in] */ __RPC__in REFGUID guidService,
        /* [in] */ __RPC__in REFIID riid,
        /* [iid_is][out] */ __RPC__deref_out_opt LPVOID *ppvObject) = 0;

};

struct MFVideoNormalizedRect;

MIDL_INTERFACE("a490b1e4-ab84-4d31-a1b2-181e03b1077a")
IMFVideoDisplayControl : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetNativeVideoSize(
        /* [unique][out][in] */ __RPC__inout_opt SIZE *pszVideo,
        /* [unique][out][in] */ __RPC__inout_opt SIZE *pszARVideo) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetIdealVideoSize(
        /* [unique][out][in] */ __RPC__inout_opt SIZE *pszMin,
        /* [unique][out][in] */ __RPC__inout_opt SIZE *pszMax) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetVideoPosition(
        /* [unique][in] */ __RPC__in_opt const MFVideoNormalizedRect *pnrcSource,
        /* [unique][in] */ __RPC__in_opt const LPRECT prcDest) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetVideoPosition(
        /* [out] */ __RPC__out MFVideoNormalizedRect *pnrcSource,
        /* [out] */ __RPC__out LPRECT prcDest) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetAspectRatioMode(
        /* [in] */ DWORD dwAspectRatioMode) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetAspectRatioMode(
        /* [out] */ __RPC__out DWORD *pdwAspectRatioMode) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetVideoWindow(
        /* [in] */ __RPC__in HWND hwndVideo) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetVideoWindow(
        /* [out] */ __RPC__deref_out_opt HWND *phwndVideo) = 0;

    virtual HRESULT STDMETHODCALLTYPE RepaintVideo( void) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetCurrentImage(
        /* [out][in] */ __RPC__inout BITMAPINFOHEADER *pBih,
        /* [size_is][size_is][out] */ __RPC__deref_out_ecount_full_opt(*pcbDib) BYTE **pDib,
        /* [out] */ __RPC__out DWORD *pcbDib,
        /* [unique][out][in] */ __RPC__inout_opt LONGLONG *pTimeStamp) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetBorderColor(
        /* [in] */ COLORREF Clr) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetBorderColor(
        /* [out] */ __RPC__out COLORREF *pClr) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetRenderingPrefs(
        /* [in] */ DWORD dwRenderFlags) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetRenderingPrefs(
        /* [out] */ __RPC__out DWORD *pdwRenderFlags) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetFullscreen(
        /* [in] */ BOOL fFullscreen) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetFullscreen(
        /* [out] */ __RPC__out BOOL *pfFullscreen) = 0;

};

typedef struct _DXVA2_VideoDesc DXVA2_VideoDesc;
typedef struct _DXVA2_ConfigPictureDecode DXVA2_ConfigPictureDecode;
struct IDirectXVideoDecoderService;
struct IDirect3DSurface9;
struct IDirect3DDeviceManager9;
struct IDirectXVideoDecoder;
