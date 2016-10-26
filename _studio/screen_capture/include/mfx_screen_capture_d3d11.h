//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_SCREEN_CAPTURE_D3D11_H__)
#define __MFX_SCREEN_CAPTURE_D3D11_H__

#include <atlbase.h>
#include <list>
#include "mfx_screen_capture_ddi.h"

namespace MfxCapture
{

class D3D11_Capturer : public Capturer
{
public:

    D3D11_Capturer(mfxCoreInterface* _core);
    virtual ~D3D11_Capturer();

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
    CComPtr<ID3D11Device>            m_pD11Device;
    CComPtr<ID3D11DeviceContext>     m_pD11Context;

    CComQIPtr<ID3D11VideoDevice>     m_pD11VideoDevice;
    CComQIPtr<ID3D11VideoContext>    m_pD11VideoContext;

    // we own video decoder, let using com pointer 
    CComPtr<ID3D11VideoDecoder>      m_pDecoder;

    UINT                             m_TargetId;

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
    D3D11_Capturer(const D3D11_Capturer& that);
    D3D11_Capturer& operator=(const D3D11_Capturer&);
};

} //namespace MfxCapture
#endif //#if !defined(__MFX_SCREEN_CAPTURE_D3D11_H__)