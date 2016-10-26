//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_SCREEN_CAPTURE_SW_D3D9_H__)
#define __MFX_SCREEN_CAPTURE_SW_D3D9_H__

#include <atlbase.h>
#include <list>
#include "mfx_screen_capture_ddi.h"
#include <d3d9.h>
#include <dxva2api.h>
#include "mfx_color_space_conversion_vpp.h"

namespace MfxCapture
{

class SW_D3D9_Capturer : public Capturer
{
public:

    SW_D3D9_Capturer(mfxCoreInterface* _core);
    virtual ~SW_D3D9_Capturer();

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
    mfxCoreParam                     m_core_par;

    mfxStatus CreateDeviceManager(const UINT AdapterID = D3DADAPTER_DEFAULT);
    mfxStatus AttachToLibraryDevice();

    std::auto_ptr<MFXVideoVPPColorSpaceConversion> m_pColorConverter;
    std::auto_ptr<OwnResizeFilter>                 m_pResizer;
    bool                      m_bResize;
    mfxU8*                    m_pResizeBuffer;
    //std::auto_ptr<CMFastCopy> m_pFastCopy;
    //bool                      m_bFastCopy;
    mfxFrameSurface1* GetFreeInternalSurface();
    mfxFrameSurface1* GetFreeIntResizeSurface();
    std::list<mfxFrameSurface1> m_InternalSurfPool;
    std::list<mfxFrameSurface1> m_InternalNV12ResizeSurfPool;
    std::list<DESKTOP_QUERY_STATUS_PARAMS> m_IntStatusList;
    mfxStatus GetDisplayResolution( mfxU32& width, mfxU32& height);
    mfxStatus CheckResolutionChage();

    mfxU32 m_width;
    mfxU32 m_height;
    mfxU32 m_CropW;
    mfxU32 m_CropH;

    mfxU32 m_DisplayIndex;

    bool                                         m_bOwnDevice;
    CComPtr<IDirect3D9                 >         m_pD3D;
    CComPtr<IDirect3DDeviceManager9    >         m_pDirect3DDeviceManager;
    CComPtr<IDirect3DDevice9           >         m_pDirect3DDevice;
    CComPtr<IDirectXVideoDecoderService>         m_pDirectXVideoService;
    HANDLE                                       m_hDirectXHandle;

private: // prohobit copy constructor and assignment operator
    SW_D3D9_Capturer(const SW_D3D9_Capturer& that);
    SW_D3D9_Capturer& operator=(const SW_D3D9_Capturer&);
};

} //namespace MfxCapture
#endif //#if !defined(__MFX_SCREEN_CAPTURE_SW_D3D9_H__)