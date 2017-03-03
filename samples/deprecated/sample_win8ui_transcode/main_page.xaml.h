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
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "pch.h"
#include "main_page.g.h"

namespace sample_win8ui_transcode
{
    public ref class MainPage sealed
    {
    public:
        MainPage();

    private:
        StorageFile^ m_pInputFile;
        StorageFile^ m_pOutputFile;
        ComPtr<IMFSourceReader> m_pSourceReader;
        ComPtr<IMFSinkWriter> m_pSinkWriter;
        ComPtr<IMFDXGIDeviceManager> m_deviceManager;
        Platform::String^ m_pOutputFileName;
        int m_iStartTime;
        int m_iEndTime;
        double m_dSearchTime;
        int m_iStreams;
        bool m_bIsCancelled;
        static Platform::String^ codecProperty;
        static Platform::String^ frameWidthProperty;
        static Platform::String^ frameHeightProperty;

        void SelectFileButtonClick(Platform::Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE);
        void TranscodeButtonClick(Platform::Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE);
        void CancelButtonClick(Platform::Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE);
        HRESULT SetDefaultParams();
        HRESULT CreateD3D11DeviceManager();
        HRESULT SetTypes();
        HRESULT SetVideoTypes(DWORD dwSourceStreamIndex);
        HRESULT SetSuitableFormat(GUID *formats, int formatNum, DWORD dwSourceStreamIndex);
        HRESULT InitializeSinkWriter(DWORD dwVideoStreamIndex);
        void addUnsupportedParam(Platform::String ^&pUnsupportedParams, Platform::String ^pParam);
        bool IsHardwareEncoderUsed(IMFTransform *pTransform);
        void GetParameters(int &iMeanBitRate, int &iQuality, int &iGOPSize, int &iBPictureCount);
        IAsyncActionWithProgress<double>^ RunTranscode();
        void ShowProgress(IAsyncActionWithProgress<double>^ pAsyncInfo, double dPercent);
        void DisplayError(Platform::String^ pError);
        void PlayFile(StorageFile^ pMediaFile);
        void EnableElements(bool bIsEnabled);
        void ClearMessages();
        void EnablePlayButtons();
        void PlayInputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE);
        void PauseInputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE);
        void StopInputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE);
        void PlayOutputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE);
        void PauseOutputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE);
        void StopOutputButtonClick(Object^ pSender, Windows::UI::Xaml::RoutedEventArgs^ pE);
    };
}
