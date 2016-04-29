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
//////////////////////////////////////////////////////////////////////////
// MainWindow.cpp: Main application window.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MainWindow.h"


const LONG        ONE_MSEC = 10000;   // The number of 100-ns in 1 msec

const UINT_PTR    IDT_TIMER1 = 1;        // Timer ID
const UINT        TICK_FREQ = 200;    // Timer frequency in msec

// Forward declarations of functions included in this code module:
void NotifyError(HWND hwnd, TCHAR* sMessage, HRESULT hrStatus);



//-----------------------------------------------------------------------------
// MainWindow constructor.
//-----------------------------------------------------------------------------

MainWindow::MainWindow() : brush(NULL), m_timerID(0), m_pPlayer(NULL)
{
}


//-----------------------------------------------------------------------------
// MainWindow destructor.
//-----------------------------------------------------------------------------

MainWindow::~MainWindow()
{
    if (brush)
    {
        DeleteObject(brush);
    }

    StopTimer();

    MSDK_SAFE_DELETE(m_pPlayer);
}

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
        EndDialog(hwndDlg, 0);
        break;
    }
    return 0;
};

//-----------------------------------------------------------------------------
// MainWindow::OnReceiveMessage
// Description: Handles window messages
//-----------------------------------------------------------------------------

LRESULT MainWindow::OnReceiveMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;

    HRESULT hr;

    switch (message)
    {

    case WM_CREATE:
        hr = OnCreate();
        if (FAILED(hr))
        {
            // Fail and quit.
            NotifyError(m_hwnd, TEXT("Cannot initialize the application."), hr);
            return -1;
        }
        break;

    case WM_SIZE:
        OnSize();
        break;

    case WM_PAINT:
        OnPaint();
        break;

    case WM_MOVE:
        OnPaint();
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_TIMER:
        OnTimer();
        break;

    case WM_NOTIFY:
        OnWmNotify((NMHDR*)lParam);
        break;

    case WM_COMMAND:
        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        switch (wmId)
        {
        case IDM_EXIT:
            DestroyWindow(m_hwnd);
            break;

        case ID_FILE_OPENFILE:
            OnFileOpen(0);
            break;

        case ID_FILE_PLAYINS3D:
            OnFileOpen(1);
            break;

        case IDC_BUTTON_PLAY:
            OnPlay();
            break;

        case IDC_BUTTON_STOP:
            OnStop();
            break;

        case IDC_BUTTON_PAUSE:
            OnPause();
            break;

        case IDC_BUTTON_MUTE:
            OnMute();
            break;

        case ID_TRANSCODEFILE_MP4_H264_MP3:
            OnFileTranscode(1);
            break;

        case ID_TRANSCODEFILE_MP4_H264_AAC:
            OnFileTranscode(2);
            break;

        case ID_TRANSCODEFILE_MPEG_TS_MPEG2_MP3:
            OnFileTranscode(3);
            break;

        case ID_TRANSCODEFILE_MPEG_TS_H264_AAC:
            OnFileTranscode(4);
            break;

        case ID_CONFIGURE_ENCODER:
            ShowFilterProperties(m_pPlayer->GetEncoder());
            break;

        case ID_CONFIGURE_RENDER:
            ShowFilterProperties(m_pPlayer->GetRender());
            break;

        case ID_HELP_ABOUT:
            DialogBox(m_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), m_hwnd, DialogProc);
            break;
        }
        break;

    // Private filter graph message.
    case WM_GRAPH_EVENT:
        hr = m_pPlayer->HandleGraphEvent(this);
        break;

    default:
        return BaseWindow::OnReceiveMessage(message, wParam, lParam);
    }
    return 0;
}


//-----------------------------------------------------------------------------
// MainWindow::OnCreate
// Description: Called when the window is created.
//-----------------------------------------------------------------------------

HRESULT MainWindow::OnCreate()
{
    HRESULT hr = S_OK;

    // Create the background brush.
    brush = CreateHatchBrush(HS_BDIAGONAL, RGB(0, 0x80, 0xFF));
    if (brush == NULL)
    {
        hr = __HRESULT_FROM_WIN32(GetLastError());
    }

    // Create the rebar control.
    if (SUCCEEDED(hr))
    {
        hr = rebar.Create(m_hInstance, m_hwnd, IDC_REBAR_CONTROL);
    }

    // Create the toolbar control.
    if (SUCCEEDED(hr))
    {
        hr = toolbar.Create(m_hInstance, m_hwnd, IDC_TOOLBAR, TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_TOP);
    }

    // Set the image list for toolbar buttons (normal state).
    if (SUCCEEDED(hr))
    {
        hr = toolbar.SetImageList(
            Toolbar::Normal,            // Image list for normal state
            IDB_TOOLBAR_IMAGES_NORMAL,    // Bitmap resource
            Size(48, 48),                // Size of each button
            5,                            // Number of buttons
            RGB(0xFF, 0x00, 0xFF)        // Color mask
            );
    }

    // Set the image list for toolbar buttons (disabled state).
    if (SUCCEEDED(hr))
    {
        hr = toolbar.SetImageList(
            Toolbar::Disabled,            // Image list for normal state
            IDB_TOOLBAR_IMAGES_DISABLED,    // Bitmap resource
            Size(48, 48),                // Size of each button
            5,                            // Number of buttons
            RGB(0xFF, 0x00, 0xFF)        // Color mask
            );
    }


    // Add buttons to the toolbar.
    if (SUCCEEDED(hr))
    {
        // Play
        hr = toolbar.AddButton(Toolbar::Button(ID_IMAGE_PLAY, IDC_BUTTON_PLAY));
    }

    if (SUCCEEDED(hr))
    {
        // Stop
        hr = toolbar.AddButton(Toolbar::Button(ID_IMAGE_STOP, IDC_BUTTON_STOP));
    }

    if (SUCCEEDED(hr))
    {
        // Pause
        hr = toolbar.AddButton(Toolbar::Button(ID_IMAGE_PAUSE, IDC_BUTTON_PAUSE));
    }

    if (SUCCEEDED(hr))
    {
        // Mute
        hr = toolbar.AddButton(Toolbar::Button(ID_IMAGE_MUTE_OFF, IDC_BUTTON_MUTE));
    }

    // Add the toolbar to the rebar control.
    if (SUCCEEDED(hr))
    {
        hr = rebar.AddBand(toolbar.Window(), 0);
    }

    //// Create the slider for seeking.

    if (SUCCEEDED(hr))
    {
        hr = Slider_Init();    // Initialize the Slider control.
    }

    if (SUCCEEDED(hr))
    {
        hr = seekbar.Create(m_hwnd, Rect(0, 0, 300, 16), IDC_SEEKBAR);
    }

    if (SUCCEEDED(hr))
    {
        hr = seekbar.SetThumbBitmap(IDB_SLIDER_THUMB);

        seekbar.SetBackground(CreateSolidBrush(RGB(239, 239, 231)));
        seekbar.Enable(FALSE);
    }

    if (SUCCEEDED(hr))
    {
        hr = rebar.AddBand(seekbar.Window(), 1);
    }

    //// Create the slider for changing the volume.

    if (SUCCEEDED(hr))
    {
        hr = volumeSlider.Create(m_hwnd, Rect(0, 0, 100, 32), IDC_VOLUME);
    }

    if (SUCCEEDED(hr))
    {
        hr = volumeSlider.SetThumbBitmap(IDB_SLIDER_VOLUME);

        volumeSlider.SetBackground(CreateSolidBrush(RGB(239, 239, 231)));
        volumeSlider.Enable(TRUE);

        // Set the range of the volume slider. In my experience, only the top half of the
        // range is audible.
        volumeSlider.SetRange(MIN_VOLUME / 2, MAX_VOLUME);
        volumeSlider.SetPosition(MAX_VOLUME);
    }

    if (SUCCEEDED(hr))
    {
        hr = rebar.AddBand(volumeSlider.Window(), 2);
    }


    // Create the tooltip

    if (SUCCEEDED(hr))
    {
        hr = toolTip.Create(m_hwnd);
    }

    if (SUCCEEDED(hr))
    {
        toolTip.AddTool(volumeSlider.Window(), L"Volume");
        toolTip.AddTool(seekbar.Window(), L"Seek");
        rebar.SendMessage(RB_SETTOOLTIPS, (WPARAM)toolTip.Window(), 0);
    }

    // Create the DirectShow player object.
    if (SUCCEEDED(hr))
    {
        RECT rcWindow;
        RECT rcControl;

        // Find the client area of the application.
        GetClientRect(m_hwnd, &rcWindow);

        // Subtract the area of the rebar control.
        GetClientRect(rebar.Window(), &rcControl);
        SubtractRect(&rcWindow, &rcWindow, &rcControl);

        video = CreateWindowEx(NULL, L"STATIC", NULL, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE ,
                                rcWindow.left, rcWindow.top, rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top,
                                m_hwnd, (HMENU)NULL, m_hInstance, NULL);

        if (video)
        {
            m_pPlayer = new DShowPlayer(video);
        }

        if (m_pPlayer == NULL)
        {
            hr = E_OUTOFMEMORY;
        }
    }

    // Set the event notification window.
    if (SUCCEEDED(hr))
    {
        hr = m_pPlayer->SetEventWindow(m_hwnd, WM_GRAPH_EVENT);
    }

    // Set default UI state.
    if (SUCCEEDED(hr))
    {
        UpdateUI();
    }

    return hr;
}



//-----------------------------------------------------------------------------
// MainWindow::OnPaint
// Description: Called when the window should be painted.
//-----------------------------------------------------------------------------

void MainWindow::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc;

    hdc = BeginPaint(m_hwnd, &ps);

    if (m_pPlayer->State() != STATE_CLOSED && m_pPlayer->HasVideo())
    {
        // The player has video, so ask the player to repaint.
        m_pPlayer->Repaint(hdc);
    }
    else
    {
        // The player does not have video. Fill in our client region, not
        // including the area for the toolbar.

        RECT rcClient;
        RECT rcToolbar;

        GetClientRect(m_hwnd, &rcClient);
        GetClientRect(rebar.Window(), &rcToolbar);

        HRGN hRgn1 = CreateRectRgnIndirect(&rcClient);
        HRGN hRgn2 = CreateRectRgnIndirect(&rcToolbar);

        CombineRgn(hRgn1, hRgn1, hRgn2, RGN_DIFF);

        FillRgn(hdc, hRgn1, brush);

        DeleteObject(hRgn1);
        DeleteObject(hRgn2);
    }

    EndPaint(m_hwnd, &ps);
}


//-----------------------------------------------------------------------------
// MainWindow::OnSize
// Description: Called when the window is resized.
//-----------------------------------------------------------------------------

void MainWindow::OnSize()
{
    // resize the toolbar
    SendMessage(toolbar.Window(), WM_SIZE, 0, 0);

    // resize the rebar
    SendMessage(rebar.Window(), WM_SIZE, 0, 0);

    RECT rcWindow;
    RECT rcControl;

    // Find the client area of the application.
    GetClientRect(m_hwnd, &rcWindow);

    // Subtract the area of the rebar control.
    GetClientRect(rebar.Window(), &rcControl);
    SubtractRect(&rcWindow, &rcWindow, &rcControl);

    // resize video window
    SetWindowPos(video, NULL, rcWindow.left, rcWindow.top, rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top, NULL);

    // What's left is the area for the video. Notify the player.
    m_pPlayer->UpdateVideoWindow(&rcWindow);
}


//-----------------------------------------------------------------------------
// MainWindow::OnTimer
// Description: Called when the timer elapses.
//-----------------------------------------------------------------------------

void MainWindow::OnTimer()
{
    // If the player can seek, update the seek bar with the current position.
    if (m_pPlayer->CanSeek())
    {
        REFERENCE_TIME timeNow;

        if (SUCCEEDED(m_pPlayer->GetCurrentPosition(&timeNow)))
        {
            seekbar.SetPosition((LONG)(timeNow / ONE_MSEC));
        }
    }
}


//-----------------------------------------------------------------------------
// MainWindow::OnWmNotify
// Description: Handle WM_NOTIFY messages.
//-----------------------------------------------------------------------------

void MainWindow::OnWmNotify(const NMHDR *pHdr)
{
    switch (pHdr->code)
    {
    case TTN_GETDISPINFO:
        // Display tool tips
        toolbar.ShowToolTip((NMTTDISPINFO*)pHdr);
        break;

    default:
        switch (pHdr->idFrom)
        {
        case IDC_SEEKBAR:
            OnSeekbarNotify((NMSLIDER_INFO*)pHdr);
            break;

        case IDC_VOLUME:
            OnVolumeSliderNotify((NMSLIDER_INFO*)pHdr);
            break;

        }
        break;
    }
}

//-----------------------------------------------------------------------------
// MainWindow::OnSeekbarNotify
// Description: Handle WM_NOTIFY messages from the seekbar.
//-----------------------------------------------------------------------------

void MainWindow::OnSeekbarNotify(const NMSLIDER_INFO *pInfo)
{
    static PlaybackState state = STATE_CLOSED;

    // Pause when the scroll action begins.
    if (pInfo->hdr.code == SLIDER_NOTIFY_SELECT)
    {
        state = m_pPlayer->State();
        m_pPlayer->Pause();
    }

    // Update the position continuously.
    m_pPlayer->SetPosition((REFERENCE_TIME)ONE_MSEC * (REFERENCE_TIME)pInfo->position);

    // Restore the state at the end.
    if (pInfo->hdr.code == SLIDER_NOTIFY_RELEASE)
    {
        if (state == STATE_STOPPED)
        {
            m_pPlayer->Stop();
        }
        else if (state == STATE_RUNNING)
        {
            m_pPlayer->Play();
        }
    }
}


//-----------------------------------------------------------------------------
// MainWindow::OnVolumeSliderNotify
// Description: Handle WM_NOTIFY messages from the volume slider.
//-----------------------------------------------------------------------------

void MainWindow::OnVolumeSliderNotify(const NMSLIDER_INFO *pInfo)
{
    m_pPlayer->SetVolume(pInfo->position);
}

void MainWindow::ShowFilterProperties(IBaseFilter * pFilter)
{
    CComQIPtr<ISpecifyPropertyPages> pProp(pFilter);

    if (NULL == pProp.p)
        return;
    // Get the filter's name and IUnknown pointer.
    FILTER_INFO FilterInfo;

    if (SUCCEEDED(pFilter->QueryFilterInfo(&FilterInfo)))
    {
        CComPtr<IUnknown> pFilterUnk;
        pFilter->QueryInterface(IID_IUnknown, (void**)&pFilterUnk);

        if (NULL != pFilterUnk.p)
        {
            // Show the page.
            CAUUID caGUID;
            if (SUCCEEDED(pProp->GetPages(&caGUID)))
            {
                OleCreatePropertyFrame(
                    m_hwnd,                 // Parent window
                    0, 0,                   // Reserved
                    FilterInfo.achName,     // Caption for the dialog box
                    1,                      // Number of objects (just the filter)
                    &pFilterUnk.p,            // Array of object pointers.
                    caGUID.cElems,          // Number of property pages
                    caGUID.pElems,          // Array of property page CLSIDs
                    0,                      // Locale identifier
                    0, NULL                 // Reserved
                    );

                CoTaskMemFree(caGUID.pElems);
            }
        }

        FilterInfo.pGraph->Release();
    }
}


void MainWindow::OnFileTranscode(INT nType)
{
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));

    WCHAR szInputFileName[MAX_PATH];
    szInputFileName[0] = L'\0';

    WCHAR szOutputFileName[MAX_PATH];
    szOutputFileName[0] = L'\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.hInstance = m_hInstance;
    ofn.lpstrFilter = L"All Files\0*.*\0\0";

    ofn.lpstrFile = szInputFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;

    HRESULT hr;
    BOOL bOpen = FALSE;

    bOpen = GetOpenFileName(&ofn);

    if (FALSE == bOpen)
    {
        return;
    }

    ofn.lpstrFile = szOutputFileName;
    ofn.Flags = 0;
    bOpen = GetSaveFileName(&ofn);

    if (FALSE == bOpen)
    {
        return;
    }

    hr = m_pPlayer->TranscodeFile(szInputFileName, szOutputFileName, nType);

    // Update the state of the UI.
    UpdateUI();

    // Invalidate the application window, in case there is an old video
    // frame from the previous file and there is no video now. (eg, the
    // new file is audio only, or we failed to open this file.)
    InvalidateRect(m_hwnd, NULL, FALSE);

    // Update the seek bar to match the current state.
    UpdateSeekBar();

    if (FAILED(hr))
    {
        NotifyError(m_hwnd, TEXT("Cannot open this file."), hr);
    }
};

//-----------------------------------------------------------------------------
// MainWindow::OnFileOpen
// Description: Open a new file for playback.
//-----------------------------------------------------------------------------

void MainWindow::OnFileOpen(INT nType)
{
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));

    WCHAR szFileName[MAX_PATH];
    szFileName[0] = L'\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.hInstance = m_hInstance;
    ofn.lpstrFilter = L"All Files\0*.*\0\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;

    HRESULT hr;

    if (GetOpenFileName(&ofn))
    {
        hr = m_pPlayer->OpenFile(szFileName, nType);

        // Update the state of the UI.
        UpdateUI();

        // Invalidate the application window, in case there is an old video
        // frame from the previous file and there is no video now. (eg, the
        // new file is audio only, or we failed to open this file.)
        InvalidateRect(m_hwnd, NULL, FALSE);

        // Update the seek bar to match the current state.
        UpdateSeekBar();

        if (SUCCEEDED(hr))
        {
            // If this file has a video stream, we need to notify
            // the VMR about the size of the destination rectangle.
            // Invoking our OnSize() handler does this.
            OnSize();
        }
        else
        {
            NotifyError(m_hwnd, TEXT("Cannot open this file."), hr);
        }

    }
}

//-----------------------------------------------------------------------------
// MainWindow::OnPlay
// Description: Start playback.
//-----------------------------------------------------------------------------

void MainWindow::OnPlay()
{
    HRESULT hr = m_pPlayer->Play();

    UpdateUI();
}

//-----------------------------------------------------------------------------
// MainWindow::OnStop
// Description: Stop playback.
//-----------------------------------------------------------------------------

void MainWindow::OnStop()
{
    HRESULT hr = m_pPlayer->Stop();

    // Seek back to the start.
    if (SUCCEEDED(hr))
    {
        if (m_pPlayer->CanSeek() || m_pPlayer->GetEncoder())
        {
            hr = m_pPlayer->SetPosition(0);
        }
    }

    UpdateUI();
}

//-----------------------------------------------------------------------------
// MainWindow::OnPause
// Description: Pause playback.
//-----------------------------------------------------------------------------

void MainWindow::OnPause()
{
    HRESULT hr = m_pPlayer->Pause();

    UpdateUI();
}


//-----------------------------------------------------------------------------
// MainWindow::OnMute
// Description: Toggle the muted / unmuted state.
//-----------------------------------------------------------------------------

void MainWindow::OnMute()
{
    if (m_pPlayer->IsMuted())
    {
        m_pPlayer->Mute(FALSE);
        toolbar.SetButtonImage(IDC_BUTTON_MUTE, ID_IMAGE_MUTE_OFF);
    }
    else
    {
        m_pPlayer->Mute(TRUE);
        toolbar.SetButtonImage(IDC_BUTTON_MUTE, ID_IMAGE_MUTE_ON);
    }
}



//-----------------------------------------------------------------------------
// MainWindow::OnGraphEvent
// Description: Callback to handle events from the filter graph.
//-----------------------------------------------------------------------------

// ! It is very important that the application does not tear down the graph inside this
// callback.

void MainWindow::OnGraphEvent(long eventCode, LONG_PTR param1, LONG_PTR param2)
{
    switch (eventCode)
    {
    case EC_COMPLETE:
        OnStop();
        break;
    }
}

//-----------------------------------------------------------------------------
// MainWindow::UpdateUI
// Description: Update the UI based on the current playback state.
//-----------------------------------------------------------------------------

void MainWindow::UpdateUI()
{
    BOOL bPlay = FALSE;
    BOOL bPause = FALSE;
    BOOL bStop = FALSE;

    switch (m_pPlayer->State())
    {
    case STATE_RUNNING:
        bPause = TRUE;
        bStop = TRUE;
        break;

    case STATE_PAUSED:
        bPlay = TRUE;
        bStop = TRUE;
        break;

    case STATE_STOPPED:
        bPlay = TRUE;
        break;
    }

    toolbar.Enable(IDC_BUTTON_PLAY, bPlay);
    toolbar.Enable(IDC_BUTTON_PAUSE, bPause);
    toolbar.Enable(IDC_BUTTON_STOP, bStop);

    HMENU hmenu    = GetMenu(m_hwnd);
    HMENU subhmenu = GetSubMenu(hmenu, 1);

    if (m_pPlayer->GetEncoder())
    {
        EnableMenuItem(subhmenu, ID_CONFIGURE_ENCODER, MF_ENABLED);
        EnableMenuItem(subhmenu, ID_CONFIGURE_RENDER, MF_GRAYED);
    }
    else
    {
        EnableMenuItem(subhmenu, ID_CONFIGURE_ENCODER, MF_GRAYED);
        EnableMenuItem(subhmenu, ID_CONFIGURE_RENDER, MF_ENABLED);
    }
}

//-----------------------------------------------------------------------------
// MainWindow::UpdateSeekBar
// Description: Update the seekbar based on the current playback state.
//-----------------------------------------------------------------------------

void MainWindow::UpdateSeekBar()
{
    // If the player can seek, set the seekbar range and start the time.
    // Otherwise, disable the seekbar.
    if (m_pPlayer->CanSeek())
    {
        seekbar.Enable(TRUE);

        LONGLONG rtDuration = 0;
        m_pPlayer->GetDuration(&rtDuration);

        seekbar.SetRange(0, (LONG)(rtDuration / ONE_MSEC));

        // Start the timer

        m_timerID = SetTimer(m_hwnd, IDT_TIMER1, TICK_FREQ, NULL);
    }
    else
    {
        seekbar.Enable(FALSE);

        // Stop the old timer, if any.
        StopTimer();
    }
}


//-----------------------------------------------------------------------------
// MainWindow::StopTimer
// Description: Stops the timer.
//-----------------------------------------------------------------------------

void  MainWindow::StopTimer()
{
    if (m_timerID != 0)
    {
        KillTimer(m_hwnd, m_timerID);
        m_timerID = 0;
    }
}


void NotifyError(HWND hwnd, TCHAR* sMessage, HRESULT hrStatus)
{
    TCHAR sTmp[512];

    HRESULT hr = StringCchPrintf(sTmp, 512, TEXT("%s hr = 0x%X"), sMessage, hrStatus);

    if (SUCCEEDED(hr))
    {
        MessageBox(hwnd, sTmp, TEXT("Error"), MB_OK | MB_ICONERROR);
    }
}


