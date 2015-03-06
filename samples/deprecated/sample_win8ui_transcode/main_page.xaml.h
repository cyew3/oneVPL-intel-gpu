//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012 Intel Corporation. All Rights Reserved.
//
//
//*/

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
