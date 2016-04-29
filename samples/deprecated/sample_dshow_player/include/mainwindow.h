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
// MainWindow.h: Main application window.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "wincontrol.h"
#include "button.h"
#include "toolbar.h"
#include "slider.h"
#include "tooltip.h"
#include "BaseWindow.h"
#include "DShowPlayer.h"
#include "resource.h"

#include <uuids.h>

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);               \
    void operator=(const TypeName&)

// TODO:
// Status bar - with size grip
// Tool tips (get rid of WTL way)
// Set minimum client area

// status bar: Display position / duration

// Make sure resources are released
// add memory check


// Handle errors from player
// Display change [TEST]
// Multimon
// frame stepping?
// Alpha bitmap?


// Some repaint problems:
//    - When open an audio-only file after a video file was opened


const UINT WM_GRAPH_EVENT = WM_APP + 1;


class MainWindow : public BaseWindow, public GraphEventCallback
{

public:
    MainWindow();
    ~MainWindow();
    LRESULT OnReceiveMessage(UINT msg, WPARAM wparam, LPARAM lparam);

    void OnGraphEvent(long eventCode, LONG_PTR param1, LONG_PTR param2);

private:
    LPCTSTR ClassName() const { return TEXT("DSHOWPLAYER"); }
    LPCTSTR MenuName() const { return MAKEINTRESOURCE(IDC_DSHOWPLAYER); }
    LPCTSTR WindowName() const { return TEXT("Dshow Player"); }

    // Message handlers
    HRESULT OnCreate();
    void    OnPaint();
    void    OnSize();
    void    OnTimer();

    // Commands
    void    OnFileOpen(INT nType = 0);
    void    OnFileTranscode(INT nType);
    void    OnPlay();
    void    OnStop();
    void    OnPause();
    void    OnMute();

    // WM_NOTIFY handlers
    void    OnWmNotify(const NMHDR *pHdr);
    void    OnSeekbarNotify(const NMSLIDER_INFO *pInfo);
    void    OnVolumeSliderNotify(const NMSLIDER_INFO *pInfo);

    void    ShowFilterProperties(IBaseFilter * pFilter);

    void    UpdateUI();
    void    UpdateSeekBar();
    void    StopTimer();

    Rebar        rebar;
    Toolbar        toolbar;
    Slider         seekbar;
    Slider         volumeSlider;
    HWND        video;
    ToolTip     toolTip;

    HBRUSH        brush;
    UINT_PTR    m_timerID;

    DShowPlayer    *m_pPlayer;

private:
    DISALLOW_COPY_AND_ASSIGN(MainWindow);
};


