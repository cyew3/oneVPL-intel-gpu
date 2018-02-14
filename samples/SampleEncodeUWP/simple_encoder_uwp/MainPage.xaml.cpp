/******************************************************************************\
Copyright (c) 2005-2018, Intel Corporation
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


using namespace simple_encoder;

using namespace Platform;
using namespace Windows::Storage;
using namespace Windows::Storage::BulkAccess;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Popups;

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::DataTransfer;

using namespace std;

MainPage::MainPage():
    m_bEncoding(false),
    m_StorageSource(nullptr),
    m_StorageSink(nullptr)
{
	InitializeComponent();
    regToken_for_ToggleEncoding = buttonRunEncoding->Click += 
        ref new RoutedEventHandler(this, &MainPage::ToggleEncodingOn);

    InvalidateControlsState();
}

void MainPage::OnAppSuspending(Object ^ sender, Windows::ApplicationModel::SuspendingEventArgs ^ e)
{
    //
}

void MainPage::OperationStatus(Platform::String ^ message)
{
    ScheduleToGuiThread([this, message]()
    {
        statusBar->Text = message;
    });
}

task<StorageFile^> MainPage::ShowFileOpenPicker()
{
    auto fileOpenPicker = ref new FileOpenPicker();
    fileOpenPicker->ViewMode = PickerViewMode::List;
    fileOpenPicker->FileTypeFilter->Clear();
    fileOpenPicker->FileTypeFilter->Append(".yuv");
    fileOpenPicker->SuggestedStartLocation = PickerLocationId::ComputerFolder;

    return create_task(fileOpenPicker->PickSingleFileAsync());
}

task<StorageFile^> MainPage::ShowFileSavePicker()
{
    auto fileSavePicker = ref new FileSavePicker();
    fileSavePicker->DefaultFileExtension = ".h264";
    auto plainTextExtensions = ref new Platform::Collections::Vector<String^>();
    plainTextExtensions->Append(fileSavePicker->DefaultFileExtension);
    fileSavePicker->FileTypeChoices->Insert("AVC/h.264 video ES", plainTextExtensions);
    fileSavePicker->SuggestedFileName = m_StorageSource->Name;

    return create_task(fileSavePicker->PickSaveFileAsync());
}

void MainPage::ToggleEncodingOn(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e)
{
    buttonRunEncoding->Click -= regToken_for_ToggleEncoding;
    regToken_for_ToggleEncoding = buttonRunEncoding->Click += 
        ref new RoutedEventHandler(this, &MainPage::ToggleEncodingOff);

    buttonRunEncoding->Content = "Stop";
    m_bEncoding = true;
    DoEncode();
}

void MainPage::ToggleEncodingOff(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e)
{
    buttonRunEncoding->Click -= regToken_for_ToggleEncoding;
    regToken_for_ToggleEncoding = buttonRunEncoding->Click += 
        ref new RoutedEventHandler(this, &MainPage::ToggleEncodingOn);

    buttonRunEncoding->Content = "Encode";
    if (m_Encoder) {
        m_Encoder->CancelEncoding();
        m_bEncoding = false;
    }
}

void MainPage::FileOpenPickerClick(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e)
{
	ShowFileOpenPicker()
		.then([this](StorageFile^ storageSource) {

		if (!storageSource) {
			cancel_current_task();
		}

		m_StorageSource = storageSource;

	})
	.then([&, this](task<void> tsk) {

		try {
			tsk.get();
            InvalidateControlsState();
            ScheduleToGuiThread([&, this]() {FileSavePickerClick(sender, e); });
		}
        catch (...) {
            HandleFilePickerException();
        }
	});
}

void MainPage::FileSavePickerClick(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e)
{
	ShowFileSavePicker()
		.then([this](StorageFile^ storageSink) {

		if (!storageSink) {
			cancel_current_task();
		}

		m_StorageSink = storageSink;

	})
	.then([this](task<void> tsk) {

		try {
			tsk.get();
            InvalidateControlsState();
		}
        catch (...) {
            HandleFilePickerException();
        }
	});
}

void MainPage::HandleFilePickerException()
{
    try {
        throw;
    }
    catch (COMException^ ex1) {

        InvalidateControlsState(
            "Error opening file, exception is: " + ex1->ToString() + "\n");
    }
    catch (Platform::Exception^ ex2) {

        InvalidateControlsState(ex2->Message);
    }
    catch (...) {
        InvalidateControlsState(
            "Error: operation aborted!");
    }
}

mfxU32 MainPage::CalculateBitrate(int frameSize)
{
    return frameSize < 414720 ? frameSize * 25 / 2592 : (frameSize - 414720) * 25 / 41088 +4000;
}

mfxVideoParam MainPage::LoadParams()
{
    mfxVideoParam Params = { 0 };

    Params.mfx.CodecId = MFX_CODEC_AVC;
    Params.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    Params.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    Params.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;

    Params.mfx.FrameInfo.FrameRateExtN = 30;
    Params.mfx.FrameInfo.FrameRateExtD = 1;

    Params.mfx.FrameInfo.CropX = 0;
    Params.mfx.FrameInfo.CropY = 0;
    Params.mfx.FrameInfo.CropW = textBoxWidth->Text->Length() ? _wtoi(textBoxWidth->Text->Begin()) : 176;
    Params.mfx.FrameInfo.CropH = textBoxHeight->Text->Length() ? _wtoi(textBoxHeight->Text->Begin()) : 144;
    Params.mfx.FrameInfo.Width = Params.mfx.FrameInfo.CropW;
    Params.mfx.FrameInfo.Height = Params.mfx.FrameInfo.CropH;

    Params.mfx.TargetKbps = Params.mfx.MaxKbps = CalculateBitrate(Params.mfx.FrameInfo.Width*Params.mfx.FrameInfo.Height);

    auto FccValue = safe_cast<TextBlock^>(FccComboBox->SelectedItem)->Text;

    if (FccValue == "NV12") {
        Params.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    }
    else if (FccValue == "NV16") {
        Params.mfx.FrameInfo.FourCC = MFX_FOURCC_NV16;
    }
    else if (FccValue == "YUV422") {
        Params.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
    }
    else if (FccValue == "YUV444") {
        Params.mfx.FrameInfo.FourCC = MFX_FOURCC_RGB4;
    }
    else if (FccValue == "P010") {
        Params.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
    }
    else if (FccValue == "P210") {
        Params.mfx.FrameInfo.FourCC = MFX_FOURCC_P210;
    }
    else if (FccValue == "RGB24") {
        Params.mfx.FrameInfo.FourCC = MFX_FOURCC_RGB3;
    }
    

    Params.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    return Params;
}

void MainPage::DoEncode()
{
	m_Encoder = CEncoder::Instantiate(LoadParams());

    if (!m_Encoder) {

        InvalidateControlsState(
            "Cold not instantiate encoder:\n\tcheck that Intel HD graphics drivers are installed correctly");
        
        buttonRunEncoding->Click -= regToken_for_ToggleEncoding;
        regToken_for_ToggleEncoding = buttonRunEncoding->Click += 
            ref new RoutedEventHandler(this, &MainPage::ToggleEncodingOn);

        buttonRunEncoding->Content = "Encode";
        m_bEncoding = false;

        return;
    }

    IAsyncActionWithProgress<double>^ encodeAction = m_Encoder->ReadAndEncodeAsync(m_StorageSource, m_StorageSink);

    encodeAction->Progress = ref new AsyncActionProgressHandler<double>(
        [this](IAsyncActionWithProgress<double>^ asyncInfo, double progressInfo) {
		
        ScheduleToGuiThread([this, progressInfo]()
		{
			progressBar->Value = progressInfo;
            OperationStatus(progressBar->Value.ToString());
		});

    });

    encodeAction->Completed = ref new AsyncActionWithProgressCompletedHandler<double>(
        [this](IAsyncActionWithProgress<double>^ asyncInfo, AsyncStatus status) {

        if (status == AsyncStatus::Completed) {

            InvalidateControlsState(
                progressBar->Value.ToString() + "% complete", 0);
			progressBar->Value = 0;
        }
        else if (status == AsyncStatus::Canceled) {

            InvalidateControlsState(
                "Encoding canceled, " + progressBar->Value.ToString() + "% complete");
        }
        else if (status == AsyncStatus::Error) {

			InvalidateControlsState(
                "Something went wrong: " + statusBar->Text);
        }

        ScheduleToGuiThread([this]()
		{
            buttonRunEncoding->Click -= regToken_for_ToggleEncoding;
            regToken_for_ToggleEncoding = buttonRunEncoding->Click += 
                ref new RoutedEventHandler(this, &MainPage::ToggleEncodingOn);

			buttonRunEncoding->Content = "Encode";
			m_bEncoding = false;
		});

        if (m_Encoder)
            m_Encoder.reset();
    });
}

void MainPage::InvalidateControlsState(Platform::String ^ state, double progress)
{
    ScheduleToGuiThread([=]() {

		textBoxSourceFileName->Text = (m_StorageSource != nullptr) ? m_StorageSource->Name : "";
		textBoxDestinationFileName->Text = (m_StorageSink != nullptr) ? m_StorageSink->Name : "";
		buttonSelectOutput->IsEnabled = !textBoxSourceFileName->Text->IsEmpty();
		buttonRunEncoding->IsEnabled = !textBoxSourceFileName->Text->IsEmpty()
			&& !textBoxDestinationFileName->Text->IsEmpty();
        if (progress >= 0) {
            if (!m_bEncoding)
                progressBar->Value = 0;
            else
                progressBar->Value = progress;
        }
	});

    OperationStatus(state);
}

inline void MainPage::ScheduleToGuiThread(std::function<void()> p) {
    Dispatcher->RunAsync(
        Windows::UI::Core::CoreDispatcherPriority::Normal, 
        ref new Windows::UI::Core::DispatchedHandler(p));
}
