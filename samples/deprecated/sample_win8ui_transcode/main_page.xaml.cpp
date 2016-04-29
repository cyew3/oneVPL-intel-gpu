/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
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

//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "main_page.xaml.h"

using namespace sample_win8ui_transcode;

#define RETURN_ON_FAIL(hr) \
{ \
    if(FAILED(hr)) \
    { \
        DisplayError("The error at the string " + (__LINE__).ToString()); \
        return hr; \
    } \
}

MainPage::MainPage()
{
    InitializeComponent();

    m_pInputFile = nullptr;
    m_pOutputFile = nullptr;
    m_pOutputFileName = TEXT("output.mp4");
}

HRESULT MainPage::CreateD3D11DeviceManager()
{
    HRESULT hr = S_OK;
    D3D_FEATURE_LEVEL level;
    ComPtr<ID3D11Device> m_pDevice;
    ComPtr<ID3D11DeviceContext> m_pDeviceContext;
    UINT32 resetToken;
    ComPtr<ID3D10Multithread> pProtected = nullptr;
    D3D_FEATURE_LEVEL levelsWanted[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    DWORD numLevelsWanted = sizeof( levelsWanted ) / sizeof( levelsWanted[0] );

    hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0 , levelsWanted, numLevelsWanted,
                            D3D11_SDK_VERSION, &m_pDevice, &level, &m_pDeviceContext);
    RETURN_ON_FAIL(hr);

    m_pDevice.Get()->QueryInterface(__uuidof(ID3D10Multithread), (void**) &pProtected);
    if (nullptr != pProtected)
    {
        pProtected->SetMultithreadProtected(TRUE);
    }

    hr = MFCreateDXGIDeviceManager(&resetToken, &m_deviceManager);
    RETURN_ON_FAIL(hr);

    hr = m_deviceManager->ResetDevice(m_pDevice.Get(), resetToken);
    RETURN_ON_FAIL(hr);

    return S_OK;
}

void MainPage::SelectFileButtonClick(Platform::Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE)
{
    outputVideoMediaElement->Pause();
    m_bIsCancelled = false;
    ClearMessages();
    try
    {
        FileOpenPicker^ pFilePicker = ref new FileOpenPicker();
        pFilePicker->SuggestedStartLocation = PickerLocationId::VideosLibrary;
        pFilePicker->FileTypeFilter->Append(".wmv");
        pFilePicker->FileTypeFilter->Append(".m4v");
        pFilePicker->FileTypeFilter->Append(".mov");
        pFilePicker->FileTypeFilter->Append(".mp4");
        pFilePicker->FileTypeFilter->Append(".avi");
        pFilePicker->FileTypeFilter->Append(".asf");

        create_task(pFilePicker->PickSingleFileAsync()).then(
            [this](StorageFile^ pInputFile)
        {
            try
            {
                m_pInputFile = pInputFile;
                if(m_pInputFile != nullptr)
                {
                    return pInputFile->OpenAsync(FileAccessMode::Read);
                }
                else
                {
                    statusTextBlock->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::Green);
                    statusTextBlock->Text += "\nPlease, select file.";
                }
            }
            catch (Platform::Exception^ pException)
            {
                DisplayError(pException->Message);
            }
            cancel_current_task();
        }).then(
            [this](IRandomAccessStream^ pInputStream)
        {
            try
            {
                HRESULT hr = S_OK;
                ComPtr<IMFByteStream> pInputByteStream;
                inputVideoMediaElement->SetSource(pInputStream, m_pInputFile->ContentType);

                hr = MFCreateMFByteStreamOnStreamEx((IUnknown*)pInputStream, &pInputByteStream);
                if (FAILED(hr))
                {
                    DisplayError("Can't create the byte stream from the input stream.");
                    cancel_current_task();
                }
                hr = CreateD3D11DeviceManager();
                if (FAILED(hr))
                {
                    DisplayError("Can't create the D3D11 device manager.");
                    cancel_current_task();
                }
                ComPtr<IMFAttributes> pMediaAttributes;
                hr = MFCreateAttributes(&pMediaAttributes,1);
                if (FAILED(hr))
                {
                    DisplayError("Can't create the attributes for the source reader.");
                    selectFileButton->IsEnabled = true;
                    cancel_current_task();
                }
                hr = pMediaAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true);
                if (FAILED(hr))
                {
                    DisplayError("Can't add the attribute to the source reader attributes.");
                    selectFileButton->IsEnabled = true;
                    cancel_current_task();
                }
                hr = pMediaAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, true);
                if (FAILED(hr))
                {
                    DisplayError("Can't add the attribute to the source reader attributes.");
                    selectFileButton->IsEnabled = true;
                    cancel_current_task();
                }
                hr = pMediaAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, m_deviceManager.Get());
                if (FAILED(hr))
                {
                    DisplayError("Can't add the attribute to the source reader attributes.");
                    selectFileButton->IsEnabled = true;
                    cancel_current_task();
                }
                hr = MFCreateSourceReaderFromByteStream(pInputByteStream.Get(), pMediaAttributes.Get(), &m_pSourceReader);
                if (FAILED(hr))
                {
                    DisplayError("Can't create the source reader from the byte stream.");
                    cancel_current_task();
                }
                hr = SetDefaultParams();
                if (FAILED(hr))
                {
                    DisplayError("Can't set default params.");
                    cancel_current_task();
                }
                EnableElements(true);
                EnablePlayButtons();
            }
            catch (Platform::Exception^ pException)
            {
                DisplayError(pException->Message);
            }
        });
    }
    catch(Exception^ pException)
    {
        DisplayError(pException->Message);
    }
}

HRESULT MainPage::SetDefaultParams()
{
    HRESULT hr = S_OK;
    ComPtr<IMFMediaType> pInputMediaType;
    UINT32 uiBitrate;

    hr = m_pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pInputMediaType);
    RETURN_ON_FAIL(hr);

    if(FAILED(pInputMediaType->GetUINT32(MF_MT_AVG_BITRATE, &uiBitrate)))
    {
        uiBitrate = MFGetAttributeUINT32(pInputMediaType.Get(), MF_MT_AVG_BITRATE, 300000);
    }

    meanBitRateTextBox->Text = uiBitrate.ToString();
    GOPSizeTextBox->Text = "0 (Default)";
    BPictureCountComboBox->SelectedIndex = 0;
    return hr;
}

void MainPage::TranscodeButtonClick(Platform::Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE)
{
    inputVideoMediaElement->Pause();
    EnableElements(false);
    cancelButton->IsEnabled = false;
    try
    {
        create_task((Windows::Storage::ApplicationData::Current)->LocalFolder->CreateFileAsync(m_pOutputFileName, CreationCollisionOption::GenerateUniqueName)).then([this](StorageFile^ pOutputFile)
        {
            try
            {
                m_pOutputFile = pOutputFile;
                if(m_pOutputFile == nullptr)
                {
                    DisplayError("Can't create output file");
                    selectFileButton->IsEnabled = true;
                    cancel_current_task();
                }
            }
            catch (Platform::Exception^ pException)
            {
                DisplayError(pException->Message);
                selectFileButton->IsEnabled = true;
                cancel_current_task();
            }
        }).then([this]
        {
            try
            {
                HRESULT hr = S_OK;
                ComPtr<IMFAttributes> pMediaAttributes;
                hr = MFCreateAttributes(&pMediaAttributes,1);
                if (FAILED(hr))
                {
                    DisplayError("Can't create the attributes for the sink writer.");
                    selectFileButton->IsEnabled = true;
                    cancel_current_task();
                }
                if (!HWModeToggleSwitch->IsOn)
                {
                    hr = pMediaAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true);
                    if (FAILED(hr))
                    {
                        DisplayError("Can't add the attribute to the sink writer attributes.");
                        selectFileButton->IsEnabled = true;
                        cancel_current_task();
                    }
                    hr = pMediaAttributes->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, m_deviceManager.Get());
                    if (FAILED(hr))
                    {
                        DisplayError("Can't add the attribute to the sink writer attributes.");
                        selectFileButton->IsEnabled = true;
                        cancel_current_task();
                    }
                }

                hr = MFCreateSinkWriterFromURL(m_pOutputFile->Path->Data(), NULL, pMediaAttributes.Get(), &m_pSinkWriter);
                if (FAILED(hr))
                {
                    DisplayError("Can't create the sink writer.");
                    selectFileButton->IsEnabled = true;
                    cancel_current_task();
                }

                hr = SetTypes();
                if (FAILED(hr))
                {
                    DisplayError("Can't set media types for SourceReader and Sink Writer.");
                    selectFileButton->IsEnabled = true;
                    cancel_current_task();
                }

                Windows::Foundation::IAsyncActionWithProgress<double>^ pTranscodeOp = RunTranscode();
                cancelButton->IsEnabled = true;

                m_iStartTime = clock();
                pTranscodeOp->Progress = ref new AsyncActionProgressHandler<double>(
                        [this](IAsyncActionWithProgress<double>^ pAsyncInfo, double dPercent){
                            ShowProgress(pAsyncInfo, dPercent);
                        }, Platform::CallbackContext::Same);

                return pTranscodeOp;
            }
            catch (Platform::Exception^ pException)
            {
                DisplayError(pException->Message);
                selectFileButton->IsEnabled = true;
                cancelButton->IsEnabled = false;
                cancel_current_task();
            }

        }).then(
            [this](task<void> transcodeTask)
        {
            try
            {
                if (m_bIsCancelled)
                {
                    DisplayError("Transcoding cancelled.");
                }
                else
                {
                    transcodeTask.get();
                    m_iEndTime = clock();
                    m_dSearchTime = (m_iEndTime - m_iStartTime)/1000.0;
                    outputTextBox->Text = "Writing complete. Working time is " + m_dSearchTime.ToString() + "sec. The output was saved to " + m_pOutputFile->Path;
                    PlayFile(m_pOutputFile);
                    cancelButton->IsEnabled = false;
                }
                selectFileButton->IsEnabled = true;
            }
            catch(Exception^ pException)
            {
                DisplayError(pException->Message);
                selectFileButton->IsEnabled = true;
            }
        });
    }
    catch(Exception^ pException)
    {
        DisplayError(pException->Message);
        selectFileButton->IsEnabled = true;
    }
}

void MainPage::CancelButtonClick(Platform::Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE)
{
    m_bIsCancelled = true;
    cancelButton->IsEnabled = false;
}

HRESULT MainPage::SetTypes()
{
    HRESULT hr = S_OK;
    DWORD dwSourceStreamIndex = 0;
    DWORD dwSinkStreamIndex = 0;
    GUID guidStreamMajorType;
    ComPtr<IMFMediaType> pStreamMediaType;
    m_iStreams = 0;
    while(SUCCEEDED(hr))
    {
        hr = m_pSourceReader->GetNativeMediaType(dwSourceStreamIndex, 0, &pStreamMediaType);
//MF_E_INVALIDSTREAMNUMBER means that Source Reader has no more streams, and we should exit from "while" loop without error
        if (hr == MF_E_INVALIDSTREAMNUMBER)
        {
            return S_OK;
        }
        RETURN_ON_FAIL(hr);

        m_iStreams++;

        hr = pStreamMediaType->GetMajorType(&guidStreamMajorType);
        RETURN_ON_FAIL(hr);

        if(guidStreamMajorType == MFMediaType_Video)
        {
            hr = SetVideoTypes(dwSourceStreamIndex);
            RETURN_ON_FAIL(hr);
        }
        else
        {
            hr = m_pSinkWriter->AddStream(pStreamMediaType.Get(), &dwSinkStreamIndex);
            RETURN_ON_FAIL(hr);
        }
        dwSourceStreamIndex++;
    }
    return hr;
}

HRESULT MainPage::SetVideoTypes(DWORD dwSourceStreamIndex)
{
    HRESULT hr = S_OK;
    ComPtr<IMFMediaType> pInputMediaType;
    ComPtr<IMFMediaType> pOutputMediaType;
    DWORD dwSinkStreamIndex = 0;
    UINT32 uiWidth, uiHeight, uiBitrate, uiNominator, uiDenominator, uiRatioX, uiRatioY, uiInterlaceMode;
    GUID guidVideoFormats[] =
    {
        MFVideoFormat_NV12,
        MFVideoFormat_I420,
        MFVideoFormat_IYUV,
        MFVideoFormat_YUY2,
        MFVideoFormat_YV12
    };

    hr = m_pSourceReader->GetCurrentMediaType(dwSourceStreamIndex, &pInputMediaType);
    RETURN_ON_FAIL(hr);

    hr = MFCreateMediaType(&pOutputMediaType);
    RETURN_ON_FAIL(hr);

    hr = MFGetAttributeSize(pInputMediaType.Get(), MF_MT_FRAME_SIZE, &uiWidth, &uiHeight);
    RETURN_ON_FAIL(hr);

    if(FAILED(pInputMediaType->GetUINT32(MF_MT_AVG_BITRATE, &uiBitrate)))
    {
        uiBitrate = MFGetAttributeUINT32(pInputMediaType.Get(), MF_MT_AVG_BITRATE, 300000);
    }

    if(FAILED(MFGetAttributeRatio(pInputMediaType.Get(), MF_MT_FRAME_RATE, &uiNominator, &uiDenominator)))
    {
        uiNominator = uiDenominator = 1;
    }

    if(FAILED(MFGetAttributeRatio(pInputMediaType.Get(), MF_MT_PIXEL_ASPECT_RATIO, &uiRatioX, &uiRatioY)))
    {
        uiRatioX = uiRatioY = 1;
    }

    if(FAILED(pInputMediaType->GetUINT32(MF_MT_INTERLACE_MODE, &uiInterlaceMode)) || uiInterlaceMode == MFVideoInterlace_Unknown)
    {
        uiInterlaceMode = MFVideoInterlace_Progressive;
    }

    hr = pOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    RETURN_ON_FAIL(hr);

    hr = pOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    RETURN_ON_FAIL(hr);

    hr = MFSetAttributeSize(pOutputMediaType.Get(), MF_MT_FRAME_SIZE, uiWidth, uiHeight);
    RETURN_ON_FAIL(hr);

    hr = MFSetAttributeRatio(pOutputMediaType.Get(), MF_MT_FRAME_RATE, uiNominator, uiDenominator);
    RETURN_ON_FAIL(hr);

    hr = pOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, uiBitrate);
    RETURN_ON_FAIL(hr);

    hr = pOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, uiInterlaceMode);
    RETURN_ON_FAIL(hr);

    hr = MFSetAttributeSize(pOutputMediaType.Get(), MF_MT_PIXEL_ASPECT_RATIO, uiRatioX, uiRatioY);
    RETURN_ON_FAIL(hr);

    hr = m_pSinkWriter->AddStream(pOutputMediaType.Get(), &dwSinkStreamIndex);
    RETURN_ON_FAIL(hr);

    hr = SetSuitableFormat(guidVideoFormats, sizeof(guidVideoFormats)/sizeof(guidVideoFormats[0]), dwSourceStreamIndex);
    RETURN_ON_FAIL(hr);

    hr = InitializeSinkWriter(dwSourceStreamIndex);
    RETURN_ON_FAIL(hr);

    if(dwSourceStreamIndex != dwSinkStreamIndex)
    {
        hr = E_UNEXPECTED;
    }
    return hr;
}

HRESULT MainPage::SetSuitableFormat(GUID *formats, int formatNum, DWORD dwSourceStreamIndex)
{
    HRESULT hr = S_OK;
    ComPtr<IMFMediaType> pPartialMediaType = nullptr;
    ComPtr<IMFMediaType> pFullMediaType = nullptr;

    hr = MFCreateMediaType(&pPartialMediaType);
    RETURN_ON_FAIL(hr);

    hr = pPartialMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    RETURN_ON_FAIL(hr);

    for(int i = 0; i < formatNum; i++)
    {
        hr = pPartialMediaType->SetGUID(MF_MT_SUBTYPE, formats[i]);
        RETURN_ON_FAIL(hr);

        hr = m_pSourceReader->SetCurrentMediaType(dwSourceStreamIndex, NULL, pPartialMediaType.Get());
        if(FAILED(hr))
        {
            continue;
        }

        hr = m_pSourceReader->GetCurrentMediaType(dwSourceStreamIndex, &pFullMediaType);
        RETURN_ON_FAIL(hr);

        hr = m_pSinkWriter->SetInputMediaType(dwSourceStreamIndex, pFullMediaType.Get(), NULL);
        if(FAILED(hr))
        {
            continue;
        }
        break;
    }
    return hr;
}

HRESULT MainPage::InitializeSinkWriter(DWORD dwVideoStreamIndex)
{
    HRESULT hr = S_OK;
    ComPtr<IMFTransform> pTransform;
    ComPtr<ICodecAPI>    pCodecAPI;
    bool bIsHardwareEncoderUsed;
    int iMeanBitRate, iQuality, iGOPSize, iBPictureCount;
    VARIANT var;
    Platform::String ^pUnsupportedParams = "";

    GetParameters(iMeanBitRate, iQuality, iGOPSize, iBPictureCount);

    hr = m_pSinkWriter->GetServiceForStream(dwVideoStreamIndex, GUID_NULL, IID_IMFTransform, (void**)&pTransform);
    RETURN_ON_FAIL(hr);

    hr = pTransform.As(&pCodecAPI);
    RETURN_ON_FAIL(hr);

    bIsHardwareEncoderUsed = IsHardwareEncoderUsed(pTransform.Get());
    if (bIsHardwareEncoderUsed)
    {
        statusTextBlock->Text = "Hardware encoder is used.";
    }
    else
    {
        statusTextBlock->Text = "";
        if (!HWModeToggleSwitch->IsOn)
            statusTextBlock->Text = "Could not load hardware encoder. ";
        statusTextBlock->Text += "Software encoder is used. ";
    }

    hr = pCodecAPI->IsModifiable(&CODECAPI_AVEncCommonMeanBitRate);
    if (hr == S_OK)
    {
        var.vt = VT_UI4;
        var.ulVal = iMeanBitRate;
        hr = pCodecAPI->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
        RETURN_ON_FAIL(hr);
    }
    else
    {
        addUnsupportedParam(pUnsupportedParams, "Mean bit rate");
    }

    hr = pCodecAPI->IsModifiable(&CODECAPI_AVEncCommonQualityVsSpeed);
    if (hr == S_OK)
    {
        var.vt = VT_UI4;
        var.ulVal = iQuality;
        hr = pCodecAPI->SetValue(&CODECAPI_AVEncCommonQualityVsSpeed, &var);
        RETURN_ON_FAIL(hr);
    }
    else
    {
        addUnsupportedParam(pUnsupportedParams, "Quality vs. speed");
    }

    hr = pCodecAPI->IsModifiable(&CODECAPI_AVEncCommonLowLatency);
    if (hr == S_OK)
    {
        var.vt = VT_BOOL;
        var.ulVal = VARIANT_FALSE;
        if (latencyModeToggleSwitch->IsOn)
            var.ulVal = VARIANT_TRUE;
        hr = pCodecAPI->SetValue(&CODECAPI_AVEncCommonLowLatency, &var);
        RETURN_ON_FAIL(hr);
    }
    else
    {
        addUnsupportedParam(pUnsupportedParams, "Low latency mode");
    }

    hr = pCodecAPI->IsModifiable(&CODECAPI_AVEncMPVGOPSize);
    if (hr == S_OK)
    {
        var.vt = VT_UI4;
        var.ulVal = iGOPSize;
        hr = pCodecAPI->SetValue(&CODECAPI_AVEncMPVGOPSize, &var);
        RETURN_ON_FAIL(hr);
    }
    else
    {
        addUnsupportedParam(pUnsupportedParams, "GOP size");
    }

    if (iBPictureCount > 0)
    {
        hr = pCodecAPI->IsModifiable(&CODECAPI_AVEncMPVProfile);
        if (hr == S_OK)
        {
            var.vt = VT_UI4;
            var.ulVal = eAVEncH264VProfile_High;
            hr = pCodecAPI->SetValue(&CODECAPI_AVEncMPVProfile, &var);
            RETURN_ON_FAIL(hr);
        }
        if (hr == S_OK)
        {
            hr = pCodecAPI->IsModifiable(&CODECAPI_AVEncMPVDefaultBPictureCount);
            if (hr == S_OK)
            {
                var.vt = VT_UI4;
                var.ulVal = iBPictureCount;
                hr = pCodecAPI->SetValue(&CODECAPI_AVEncMPVDefaultBPictureCount, &var);
                RETURN_ON_FAIL(hr);
            }
        }
        if (hr != S_OK)
        {
            addUnsupportedParam(pUnsupportedParams, "B picture count");
        }
    }

    if (pUnsupportedParams != "")
        statusTextBlock->Text += "Encoder does not support " + pUnsupportedParams + " parameters.";

    return S_OK;
}

void MainPage::addUnsupportedParam(Platform::String ^&pUnsupportedParams, Platform::String ^pParam)
{
    if (pUnsupportedParams != "")
        pUnsupportedParams += ", ";
    pUnsupportedParams += pParam;
}

bool MainPage::IsHardwareEncoderUsed(IMFTransform *pTransform)
{
    ComPtr<IMFAttributes> pAttributes;
    HRESULT hr = pTransform->GetAttributes(&pAttributes);
    if (FAILED(hr))
    {
        return false;
    }

    UINT32 uiLength = 0;
    Platform::String ^pStr;
    hr = pAttributes->GetStringLength(MFT_ENUM_HARDWARE_URL_Attribute, &uiLength);
    if (FAILED(hr))
    {
        return false;
    }

    pStr = ref new Platform::String(L"", uiLength + 1);
    if (pStr == nullptr)
    {
        return false;
    }

    hr = pAttributes->GetString(MFT_ENUM_HARDWARE_URL_Attribute, (LPWSTR) pStr->Data(), uiLength + 1, &uiLength);
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void MainPage::GetParameters(int &iMeanBitRate, int &iQuality, int &iGOPSize, int &iBPictureCount)
{
    iMeanBitRate = _wtoi(meanBitRateTextBox->Text->Data());
    if (iMeanBitRate <= 0 || iMeanBitRate >= UINT_MAX)
        iMeanBitRate = 300000;
    iQuality = (int) qualitySlider->Value;
    iGOPSize = _wtoi(GOPSizeTextBox->Text->Data());
    if (iGOPSize < 0 || iGOPSize >= UINT_MAX)
        iGOPSize = 256;
    iBPictureCount = BPictureCountComboBox->SelectedIndex;
}

IAsyncActionWithProgress<double>^ MainPage::RunTranscode()
{
    return create_async([this](progress_reporter<double> reporter)
    {
        HRESULT hr = S_OK;
        ComPtr<IMFSample> pSample;
        DWORD dwStreamIndex = 0, dwFlags = 0;
        LONGLONG llTimestamp = 0, llSampleTime = 0, llFileDuration;
        int iFinishedStreams = 0;
        PROPVARIANT var;
        double dPercents = 0;
        hr = m_pSourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);
        llFileDuration = ((LARGE_INTEGER) var.hVal).QuadPart;
        hr = m_pSinkWriter->BeginWriting();
        if (FAILED(hr))
        {
            cancel_current_task();
        }
        while(iFinishedStreams < m_iStreams)
        {
            hr = m_pSourceReader->ReadSample(
                (DWORD)MF_SOURCE_READER_ANY_STREAM,
                0,
                &dwStreamIndex,
                &dwFlags,
                &llTimestamp,
                &pSample);
            if (FAILED(hr))
            {
                break;
            }

            if (dwFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
            {
                ComPtr<IMFMediaType> pInputMediaType;
                hr = m_pSourceReader->GetCurrentMediaType((DWORD) MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pInputMediaType);
                if(FAILED(hr))
                {
                    break;
                }
                hr = m_pSinkWriter->SetInputMediaType(dwStreamIndex, pInputMediaType.Get(), NULL);
                if(FAILED(hr))
                {
                    break;
                }
            }

            if( pSample != NULL )
            {
                hr = m_pSinkWriter->WriteSample(dwStreamIndex, pSample.Get());
                if (FAILED(hr))
                {
                    break;
                }
            }
            if(dwFlags & MF_SOURCE_READERF_STREAMTICK)
            {
                hr = m_pSinkWriter->SendStreamTick(dwStreamIndex, llTimestamp);
                if (FAILED(hr))
                {
                    break;
                }
            }
            if(dwFlags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                hr = m_pSinkWriter->NotifyEndOfSegment(dwStreamIndex);
                if (FAILED(hr))
                {
                    break;
                }
                iFinishedStreams++;
            }
            if( pSample != NULL && llFileDuration > 0)
            {
                pSample->GetSampleTime(&llSampleTime);
                dPercents = (double) llSampleTime / llFileDuration * 100.0f;
                if (dPercents > 0)
                    reporter.report(dPercents);
            }
            if (m_bIsCancelled)
                break;
        }
        m_pSinkWriter->Finalize();
    });
}

void MainPage::ShowProgress(IAsyncActionWithProgress<double>^ pAsyncInfo, double dPercent)
{
    outputTextBox->Text = "Progress:  " + ((int)dPercent).ToString() + "%";
}

void MainPage::DisplayError(Platform::String^ pError)
{
    statusTextBlock->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::Red);
    statusTextBlock->Text += "\n";
    statusTextBlock->Text += pError;
}

void MainPage::PlayFile(StorageFile^ pMediaFile)
{
    try
    {
        create_task(pMediaFile->OpenAsync(FileAccessMode::Read)).then(
            [&, this, pMediaFile](IRandomAccessStream^ outputStream)
        {
            try
            {
                outputVideoMediaElement->SetSource(outputStream, pMediaFile->ContentType);
            }
            catch (Platform::Exception^ pException)
            {
                DisplayError(pException->Message);
            }
        });
    }
    catch (Exception^ pException)
    {
        DisplayError(pException->Message);
    }
}

void MainPage::EnableElements(bool bIsEnabled)
{
    selectFileButton->IsEnabled = bIsEnabled;
    meanBitRateTextBox->IsEnabled = bIsEnabled;
    qualitySlider->IsEnabled = bIsEnabled;
    transcodeButton->IsEnabled = bIsEnabled;
    latencyModeToggleSwitch->IsEnabled = bIsEnabled;
    GOPSizeTextBox->IsEnabled = bIsEnabled;
    BPictureCountComboBox->IsEnabled = bIsEnabled;
    HWModeToggleSwitch->IsEnabled = bIsEnabled;
}

void MainPage::ClearMessages()
{
    statusTextBlock->Text = "";
    statusTextBlock->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::White);
    outputTextBox->Text = "";
}

void MainPage::EnablePlayButtons()
{
    playInputButton->IsEnabled = true;
    pauseInputButton->IsEnabled = true;
    stopInputButton->IsEnabled = true;
    playOutputButton->IsEnabled = true;
    pauseOutputButton->IsEnabled = true;
    stopOutputButton->IsEnabled = true;
}

void MainPage::PlayInputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE)
{
    inputVideoMediaElement->Play();
}

void MainPage::PauseInputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE)
{
    inputVideoMediaElement->Pause();
}

void MainPage::StopInputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE)
{
    inputVideoMediaElement->Stop();
}

void MainPage::PlayOutputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE)
{
    outputVideoMediaElement->Play();
}

void MainPage::PauseOutputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE)
{
    outputVideoMediaElement->Pause();
}

void MainPage::StopOutputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE)
{
    outputVideoMediaElement->Stop();
}
