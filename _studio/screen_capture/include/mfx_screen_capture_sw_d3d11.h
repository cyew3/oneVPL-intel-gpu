/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_sw_d3d11.h

\* ****************************************************************************** */

#if !defined(__MFX_SCREEN_CAPTURE_SW_D3D11_H__)
#define __MFX_SCREEN_CAPTURE_SW_D3D11_H__

#include <atlbase.h>
#include <list>
#include "mfx_screen_capture_ddi.h"
#include "DXGI1_2.h"

namespace MfxCapture
{

class SW_D3D11_Capturer : public Capturer
{
public:

    SW_D3D11_Capturer(mfxCoreInterface* _core);
    virtual ~SW_D3D11_Capturer();

    virtual mfxStatus CreateVideoAccelerator(mfxVideoParam const & par);
    virtual mfxStatus QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out);
    virtual mfxStatus CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out);
    virtual mfxStatus Destroy();

    virtual mfxStatus BeginFrame( mfxFrameSurface1* pSurf );
    virtual mfxStatus EndFrame( );
    virtual mfxStatus GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber);
    virtual mfxStatus QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList);

protected:
    mfxCoreInterface*                m_pmfxCore;

};

} //namespace MfxCapture
#endif //#if !defined(__MFX_SCREEN_CAPTURE_SW_D3D11_H__)