/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_sw_d3d9.h

\* ****************************************************************************** */

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

    virtual mfxStatus CreateVideoAccelerator(mfxVideoParam const & par);
    virtual mfxStatus QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out);
    virtual mfxStatus CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out);
    virtual mfxStatus Destroy();

    virtual mfxStatus BeginFrame( mfxMemId MemId);
    virtual mfxStatus EndFrame( );
    virtual mfxStatus GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber);
    virtual mfxStatus QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList);

protected:
    mfxCoreInterface*                m_pmfxCore;

    mfxStatus CreateDeviceManager();

    std::auto_ptr<MFXVideoVPPColorSpaceConversion> m_pColorConverter;
    mfxFrameSurface1* GetFreeInternalSurface();
    std::list<mfxFrameSurface1> m_InternalSurfPool;
    std::list<DESKTOP_QUERY_STATUS_PARAMS> m_IntStatusList;

    CComPtr<IDirect3D9                 >         m_pD3D;
    CComPtr<IDirect3DDeviceManager9    >         m_pDirect3DDeviceManager;
    CComPtr<IDirect3DDevice9           >         m_pDirect3DDevice;
    CComPtr<IDirectXVideoDecoderService>         m_pDirectXVideoService;

};

} //namespace MfxCapture
#endif //#if !defined(__MFX_SCREEN_CAPTURE_SW_D3D9_H__)