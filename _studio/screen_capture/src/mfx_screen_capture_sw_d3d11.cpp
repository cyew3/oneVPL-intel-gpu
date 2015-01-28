/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_d3d11.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_sw_d3d11.h"
#include "mfx_utils.h"

namespace MfxCapture
{

SW_D3D11_Capturer::SW_D3D11_Capturer(mfxCoreInterface* _core)
    :m_pmfxCore(_core)
{

}

SW_D3D11_Capturer::~SW_D3D11_Capturer()
{
    Destroy();
}

mfxStatus SW_D3D11_Capturer::CreateVideoAccelerator( mfxVideoParam const & par)
{
    par;
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus SW_D3D11_Capturer::QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out)
{
    in;
    out;
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus SW_D3D11_Capturer::CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out)
{
    in;
    out;
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus SW_D3D11_Capturer::Destroy()
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus SW_D3D11_Capturer::BeginFrame( mfxMemId MemId)
{
    MemId;
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus SW_D3D11_Capturer::EndFrame( )
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus SW_D3D11_Capturer::GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber)
{
    surface_work;
    StatusReportFeedbackNumber;
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus SW_D3D11_Capturer::QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList)
{
    StatusList;
    return MFX_ERR_UNSUPPORTED;
}

} //namespace MfxCapture