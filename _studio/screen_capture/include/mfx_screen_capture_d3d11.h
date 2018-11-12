// Copyright (c) 2015-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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