//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_SCREEN_CAPTURE_D3D9_H__)
#define __MFX_SCREEN_CAPTURE_D3D9_H__

#include <atlbase.h>
#include <list>
#include "mfx_screen_capture_ddi.h"

#include <dxva2api.h>

#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N','V','1','2')

static const GUID DXVA2_Intel_Auxiliary_Device = 
{ 0xa74ccae2, 0xf466, 0x45ae, { 0x86, 0xf5, 0xab, 0x8b, 0xe8, 0xaf, 0x84, 0x83 } };

// From "Intel DXVA2 Auxiliary Functionality Device rev 0.6"
typedef enum
{
    AUXDEV_GET_ACCEL_GUID_COUNT             = 1,
    AUXDEV_GET_ACCEL_GUIDS                  = 2,
    AUXDEV_GET_ACCEL_RT_FORMAT_COUNT        = 3,
    AUXDEV_GET_ACCEL_RT_FORMATS             = 4,
    AUXDEV_GET_ACCEL_FORMAT_COUNT           = 5,
    AUXDEV_GET_ACCEL_FORMATS                = 6,
    AUXDEV_QUERY_ACCEL_CAPS                 = 7,
    AUXDEV_CREATE_ACCEL_SERVICE             = 8,
    AUXDEV_DESTROY_ACCEL_SERVICE            = 9
} AUXDEV_FUNCTION_ID;

namespace MfxCapture
{

class D3D9_Capturer : public Capturer
{
public:

    D3D9_Capturer(mfxCoreInterface* _core);
    virtual ~D3D9_Capturer();

    virtual mfxStatus CreateVideoAccelerator(mfxVideoParam const & par, const mfxU32 targetId = 0);
    virtual mfxStatus QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out, const mfxU32 targetId = 0);
    virtual mfxStatus CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out);
    virtual mfxStatus Destroy();

    virtual mfxStatus BeginFrame( mfxFrameSurface1* pSurf );
    virtual mfxStatus EndFrame( );
    virtual mfxStatus GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber);
    virtual mfxStatus QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList);

protected:
    mfxCoreInterface*                m_pmfxCore;

    CComPtr<IDirect3D9                 >         m_pD3D;
    CComPtr<IDirect3DDeviceManager9    >         m_pDirect3DDeviceManager;
    CComPtr<IDirect3DDevice9           >         m_pDirect3DDevice;
    CComPtr<IDirectXVideoDecoderService>         m_pDirectXVideoService;
    CComPtr<IDirectXVideoDecoder>                m_pDecoder;
    HANDLE                                       m_hDirectXHandle;
    UINT                                         m_TargetId;

    //WA for implicit resize
#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    bool              m_bImplicitResizeWA;
    mfxU32            m_AsyncDepth;
    mfxU32            m_CropW;
    mfxU32            m_CropH;
    mfxStatus         ResetInternalSurfaces(const mfxU32& n, mfxFrameInfo* frameInfo);
    mfxFrameSurface1* GetFreeInternalSurface();
    std::list<mfxFrameSurface1> m_InternalSurfPool;
#endif

private: // prohobit copy constructor and assignment operator
    D3D9_Capturer(const D3D9_Capturer& that);
    D3D9_Capturer& operator=(const D3D9_Capturer&);
};

} //namespace MfxCapture
#endif //#if !defined(__MFX_SCREEN_CAPTURE_D3D9_H__)