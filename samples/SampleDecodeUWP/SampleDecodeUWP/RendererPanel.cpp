/******************************************************************************\
Copyright (c) 2005-2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "pch.h"
#include "RendererPanel.h"
#include "Windows.ui.xaml.media.dxinterop.h"

using namespace SampleDecodeUWP;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Documents;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;

// The Templated Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234235

CRendererPanel::CRendererPanel()
{
    //this->SizeChanged += ref new Windows::UI::Xaml::SizeChangedEventHandler(this, &SampleDecodeUWP::CRendererPanel::OnSizeChanged);
    this->LayoutUpdated += ref new Windows::Foundation::EventHandler<Platform::Object ^>(this, &SampleDecodeUWP::CRendererPanel::OnLayoutUpdated);
}


//void CRendererPanel::OnSizeChanged(Platform::Object ^sender, Windows::UI::Xaml::SizeChangedEventArgs ^e)
//{
//    renderer.Resize((UINT)e->NewSize.Width, (UINT)e->NewSize.Height);
//}

bool CRendererPanel::Init(IntPtr pDevHdl)
{
    mfxStatus sts = renderer.CreateSwapChain((void*)pDevHdl,320,240,false); // Swapchain buffers will be resized later
    MSDK_CHECK_STATUS_BOOL(sts, "Cannot create rendering swapchain");

    // Assign swapchain to SwapChainPanel
    CComPtr<ISwapChainPanelNative> swapChainNative;
    IInspectable* panelInspectable = (IInspectable*) reinterpret_cast<IInspectable*>(this);
    panelInspectable->QueryInterface(__uuidof(ISwapChainPanelNative), (void **)&swapChainNative);
    HRESULT hr = swapChainNative->SetSwapChain(renderer.GetSwapChain());
    return true;
}


void SampleDecodeUWP::CRendererPanel::OnLayoutUpdated(Platform::Object ^sender, Platform::Object ^args)
{
    if (this->ActualWidth > 0 && this->ActualHeight > 0)
    {
        renderer.Resize((UINT)this->ActualWidth, (UINT)this->ActualHeight);
    }
}
