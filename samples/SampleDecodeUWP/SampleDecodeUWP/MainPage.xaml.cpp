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
#include "MainPage.xaml.h"

using namespace Windows::UI::Popups;
using namespace SampleDecodeUWP;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace concurrency;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

MainPage::MainPage()
{
	InitializeComponent();
    btnPlay->IsEnabled = false;
    btnPause->IsEnabled = false;
    btnStop->IsEnabled = false;

    //--- Attaching event handler (it will execute OnPipelineStatusChanged event handler in GUI context)
    auto window = Windows::UI::Core::CoreWindow::GetForCurrentThread();
    auto dispatcher = window->Dispatcher;
    pipeline.OnPipelineStatusChanged = [this,dispatcher](CDecodingPipeline::PIPELINE_STATUS st)
    {
        dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
            ref new Windows::UI::Core::DispatchedHandler([this,st]
        {
            this->OnPipelineStatusChanged(st);
        })); // end m_dispatcher.RunAsync
    };
}


void SampleDecodeUWP::MainPage::btnPlay_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    pipeline.Play();
}

void SampleDecodeUWP::MainPage::Page_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    if (!pipeline.Init())
    {
        ShowErrorAndExit("Cannot initialize library");
    }

    if (!RendererPanel->Init(pipeline.GetHWDevHdl()))
    {
        ShowErrorAndExit("Cannot create swapchain for rendering");
    }
    pipeline.SetRendererPanel(RendererPanel);

    TimeSpan ts;
    ts.Duration = 500;
    timer->Interval = ts;
    timer->Tick += ref new Windows::Foundation::EventHandler<Platform::Object ^>(this, &SampleDecodeUWP::MainPage::OnTick);
    timer->Start();
}


void SampleDecodeUWP::MainPage::btnOpen_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    Windows::Storage::Pickers::FileOpenPicker^ picker = ref new Windows::Storage::Pickers::FileOpenPicker();
    picker->ViewMode = Windows::Storage::Pickers::PickerViewMode::Thumbnail;
    picker->SuggestedStartLocation = Windows::Storage::Pickers::PickerLocationId::ComputerFolder;
    picker->FileTypeFilter->Append(".h264");
    picker->FileTypeFilter->Append(".264");
    picker->FileTypeFilter->Append(".264");
    picker->FileTypeFilter->Append(".m2v");
    picker->FileTypeFilter->Append(".mpg");
    picker->FileTypeFilter->Append(".bs");
    picker->FileTypeFilter->Append(".es");
    // No support for HEVC in UWP version of library so far because plugins are unsupported
//    picker->FileTypeFilter->Append(".h265");
//    picker->FileTypeFilter->Append(".265");
//    picker->FileTypeFilter->Append(".hevc");

    create_task(picker->PickSingleFileAsync()).then([this](Windows::Storage::StorageFile^ file)
    {
        if (file)
        {
            if(file->FileType==".264" || file->FileType == ".h264")
            {
                pipeline.SetCodecID(MFX_CODEC_AVC);
            }
            else if (file->FileType == ".m2v" || file->FileType == ".mpg" || file->FileType == ".bs" || file->FileType == ".es")
            {
                pipeline.SetCodecID(MFX_CODEC_MPEG2);
            }
            //else if (file->FileType == ".265" || file->FileType == ".h265" || file->FileType == ".hevc")
            //{
            //    pipeline.SetCodecID(MFX_CODEC_HEVC);
            //}
            else
            {
                ShowMessage("Invalid type of file selected", [](auto param){});
                return;
            }
            pipeline.Stop();
            pipeline.Load(file);
            pipeline.Play();
        }
    });
}

void SampleDecodeUWP::MainPage::btnStop_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    pipeline.Stop();
}

void SampleDecodeUWP::MainPage::OnPipelineStatusChanged(CDecodingPipeline::PIPELINE_STATUS status)
{
    switch(status)
    {
    case CDecodingPipeline::PS_NONE:
        btnPlay->IsEnabled = false;
        btnPause->IsEnabled = false;
        btnStop->IsEnabled = false;
        btnOpen->IsEnabled = true;

    case CDecodingPipeline::PS_STOPPED:
        btnPlay->IsEnabled = true;
        btnPause->IsEnabled = false;
        btnStop->IsEnabled = false;
        btnOpen->IsEnabled = true;
        break;
    case CDecodingPipeline::PS_PAUSED:
        btnPlay->IsEnabled = true;
        btnPause->IsEnabled = false;
        btnStop->IsEnabled = true;
        btnOpen->IsEnabled = false;
        break;
    case CDecodingPipeline::PS_PLAYING:
        btnPlay->IsEnabled = false;
        btnPause->IsEnabled = true;
        btnStop->IsEnabled = true;
        btnOpen->IsEnabled = false;
        break;
    }
}


void SampleDecodeUWP::MainPage::btnPause_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    pipeline.Pause();
}

void SampleDecodeUWP::MainPage::RendererPanel_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e)
{
    GeneralTransform^ tr = btnOpen->TransformToVisual(AppGrid);
    AppGrid->Width= tr->TransformPoint(Point(0, 0)).X;
}


void SampleDecodeUWP::MainPage::OnTick(Platform::Object ^sender, Platform::Object ^args)
{
    progressSlider->Value = pipeline.GetProgressPromilleage();
}
