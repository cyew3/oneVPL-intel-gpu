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

#if !defined(__MFX_SCREEN_CAPTURE_SW_D3D11_H__)
#define __MFX_SCREEN_CAPTURE_SW_D3D11_H__

#include <atlbase.h>
#include <list>
#include <umc_mutex.h>
#include "mfx_screen_capture_ddi.h"
#include "mfx_color_space_conversion_vpp.h"

#include <initguid.h>
#include "DXGI1_2.h"
#include "D3D11_1.h"

namespace MfxCapture
{

class SW_D3D11_Capturer : public Capturer
{
public:

    SW_D3D11_Capturer(mfxCoreInterface* _core);
    virtual ~SW_D3D11_Capturer();

    virtual mfxStatus CreateVideoAccelerator(mfxVideoParam const & par, const mfxU32 targetId = 0);
    virtual mfxStatus QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out, const mfxU32 targetId = 0);
    virtual mfxStatus CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out);
    virtual mfxStatus Destroy();

    virtual mfxStatus BeginFrame( mfxFrameSurface1* pSurf );
    virtual mfxStatus EndFrame( );
    virtual mfxStatus GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber);
    virtual mfxStatus QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList);

protected:
    mfxU32 m_DisplayIndex;

    mfxCoreInterface*                m_pmfxCore;
    mfxCoreParam                     m_core_par;

    CComPtr<IDXGIFactory2>           m_pDXGIFactory;
    CComPtr<IDXGIAdapter1>           m_pDXGIAdapter;
    CComPtr<IDXGIOutput>             m_pDXGIOutput;
    CComPtr<IDXGIOutput1>            m_pDXGIOutput1;
    CComPtr<IDXGIOutputDuplication>  m_pDXGIOutputDuplication;

    CComPtr<ID3D11Device>            m_pD11Device;
    //CComPtr<ID3D11Device1>           m_pD11Device1;

    CComPtr<IDXGIDevice2>            m_pDXGIDevice;

    CComPtr<ID3D11DeviceContext>     m_pD11Context;

    CComPtr<IDXGIResource>           m_pDesktopResource;
    CComPtr<ID3D11Texture2D>         m_pLastCaptured;
    CComPtr<ID3D11Texture2D>         m_stagingTexture;

    bool                             m_bOwnDevice;

    std::list<DESKTOP_QUERY_STATUS_PARAMS> m_IntStatusList;
    std::auto_ptr<MFXVideoVPPColorSpaceConversion> m_pColorConverter;
    //std::list<mfxFrameSurface1> m_InternalSurfPool;
    //std::list<mfxFrameSurface1> m_InternalNV12ResizeSurfPool;

    mfxStatus CreateAdapter(const displaysDescr& ActiveDisplays, const UINT OutputId = 0);
    mfxStatus AttachToLibraryDevice(const displaysDescr& ActiveDisplays, const UINT OutputId = 0);

    UMC::Mutex m_guard;

private: // prohobit copy constructor and assignment operator
    SW_D3D11_Capturer(const SW_D3D11_Capturer& that);
    SW_D3D11_Capturer& operator=(const SW_D3D11_Capturer&);
};

} //namespace MfxCapture
#endif //#if !defined(__MFX_SCREEN_CAPTURE_SW_D3D11_H__)