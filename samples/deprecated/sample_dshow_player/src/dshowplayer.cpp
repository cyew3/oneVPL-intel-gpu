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

#include "mfx_samples_config.h"

//////////////////////////////////////////////////////////////////////////
// DShowPlayer.cpp: Implements DirectShow playback functionality.
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
#include "DShowPlayer.h"
#include "DshowUtil.h"
#include "dvdmedia.h"
#include "wmsysprf.h"

#include <dshow.h>
#include <initguid.h>
#include <qnetwork.h>

HRESULT RemoveUnconnectedRenderer(IGraphBuilder *pGraph, IBaseFilter *pRenderer, BOOL *pbRemoved);
HRESULT InitWindowlessVMR(IBaseFilter *pVMR, HWND hwnd, IMFVideoDisplayControl** ppWc);

//-----------------------------------------------------------------------------
// DShowPlayer constructor.
//-----------------------------------------------------------------------------

DShowPlayer::DShowPlayer(HWND hwndVideo) :
  m_state(STATE_CLOSED),
  m_hwndVideo(hwndVideo),
  m_hwndEvent(NULL),
  m_EventMsg(0),
  m_pGraph(NULL),
  m_pControl(NULL),
  m_pEvent(NULL),
  m_pSeek(NULL),
  m_pWindowless(NULL),
    m_pCustomPresenter(NULL),
  m_pAudio(NULL),
  m_seekCaps(0),
  m_bMute(FALSE),
  m_lVolume(MAX_VOLUME)
{


}

//-----------------------------------------------------------------------------
// DShowPlayer destructor.
//-----------------------------------------------------------------------------

DShowPlayer::~DShowPlayer()
{
  TearDownGraph();
}



//-----------------------------------------------------------------------------
// DShowPlayer::SetEventWindow
// Description: Set the window to receive graph events.
//
// hwnd: Window to receive the events.
// msg: Private window message that window will receive whenever a
//      graph event occurs. (Must be in the range WM_APP through 0xBFFF.)
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::SetEventWindow(HWND hwnd, UINT msg)
{
  m_hwndEvent = hwnd;
  m_EventMsg = msg;
  return S_OK;
}

HRESULT DShowPlayer::TranscodeFile(const WCHAR* sSrcFileName, const WCHAR* sDstFileName, INT nType)
{
    HRESULT hr = S_OK;

    IBaseFilter *pSource       = NULL;
    IBaseFilter* pSplitter     = NULL;
    IBaseFilter* pVideoDecoder = NULL;
    IBaseFilter* pAudioDecoder = NULL;
    IBaseFilter* pVideoEncoder = NULL;
    IBaseFilter* pAudioEncoder = NULL;
    IBaseFilter* pMuxer        = NULL;
    IBaseFilter* pFileWriter   = NULL;

  IFileSourceFilter *pFileSourceAsync = NULL;
    IFileSourceFilter* pWNMASFReader = NULL;

    // Create a new filter graph. (This also closes the old one, if any.)
    hr = InitializeGraph();

    // Add the source filter to the graph.
  if (SUCCEEDED(hr))
  {
        hr = AddFilterByCLSID(m_pGraph, CLSID_AsyncReader, &pSource, L"File Source Async");
  }

    if (SUCCEEDED(hr))
    {
        pSource->QueryInterface(IID_IFileSourceFilter, (void**)&pFileSourceAsync);
        hr = pFileSourceAsync->Load(sSrcFileName, NULL);

        MSDK_SAFE_RELEASE(pFileSourceAsync);
    }

    //connect source to splitter
    if (SUCCEEDED(hr))
    {
        hr = ConnectFilterToFilter(pSource, &pSplitter, SPLITTER);
    }

    //if connect to splitter failed suppose that file is wmv
    if (FAILED(hr) && pSource)
    {
        LPOLESTR strFileName = NULL;

        hr = pSource->QueryInterface(IID_IFileSourceFilter, (void**)&pWNMASFReader);

        if (SUCCEEDED(hr))
        {
            hr = pWNMASFReader->GetCurFile(&strFileName, NULL);
            MSDK_SAFE_RELEASE(pWNMASFReader);
        }

        if (SUCCEEDED(hr))
        {
            hr = m_pGraph->RemoveFilter(pSource);
        }

        if (SUCCEEDED(hr))
        {
            hr = AddFilterByCLSID(m_pGraph, CLSID_WMAsfReader, &pSplitter, L"ASF Reader");
        }

        if (SUCCEEDED(hr))
        {
            hr = pSplitter->QueryInterface(IID_IFileSourceFilter, (void**)&pWNMASFReader);
        }

        if (SUCCEEDED(hr))
        {
            hr = pWNMASFReader->Load(strFileName, NULL);
        }
    }

    //connect splitter to video, audio
    if (SUCCEEDED(hr))
    {
        HRESULT hr2 = E_FAIL;

        hr = E_FAIL;

        //video
        hr2 = ConnectFilterToFilter(pSplitter, &pVideoDecoder, VIDEO_DECODER);
        if (SUCCEEDED(hr2))
        {
            hr = S_OK;
        }
        else
        {
            MSDK_SAFE_RELEASE(pVideoDecoder);
        }

        hr2 = E_FAIL;

        //audio
        hr2 = ConnectFilterToFilter(pSplitter, &pAudioDecoder, AUDIO_DECODER);

        //try to connect with WMAudio DMO
        if (FAILED(hr2))
        {
            hr2 = ConnectToDMOAudio(pSplitter, &pAudioDecoder);
        }

        if (SUCCEEDED(hr2))
        {
            hr = S_OK;
        }
        else
        {
            MSDK_SAFE_RELEASE(pAudioDecoder);
        }
    }

    //connect decoders to encoders
    if (SUCCEEDED(hr))
    {
        HRESULT hr2 = E_FAIL;

        hr = E_FAIL;

        if (pVideoDecoder)
        {
      hr2 = AddFilterByCLSID(m_pGraph, guidVideoEncoders[nType-1], &pVideoEncoder, L"Video Encoder");

            if (SUCCEEDED(hr2))
            {
                hr2 = ConnectFilters(m_pGraph, pVideoDecoder, pVideoEncoder);

                if (FAILED(hr2))
                {
                    m_pGraph->RemoveFilter(pVideoEncoder);
                    MSDK_SAFE_RELEASE(pVideoEncoder);
                }
            }
        }

        if (SUCCEEDED(hr2))
        {
            hr = S_OK;
        }

        hr2 = E_FAIL;

        if (pAudioDecoder)
        {
            hr2 = AddFilterByCLSID(m_pGraph, guidAudioEncoders[nType-1], &pAudioEncoder, L"Audio Encoder");

            if (SUCCEEDED(hr2))
            {
                hr2 = ConnectFilters(m_pGraph, pAudioDecoder, pAudioEncoder);

                if (FAILED(hr2))
                {
                    m_pGraph->RemoveFilter(pAudioEncoder);
                    MSDK_SAFE_RELEASE(pAudioEncoder);
                }
            }
        }

        if (SUCCEEDED(hr2))
        {
            hr = S_OK;
        }
    }

    //connect encoders to muxer
    if (SUCCEEDED(hr))
    {
        HRESULT hr2 = E_FAIL;

        hr = AddFilterByCLSID(m_pGraph, guidMuxers[nType-1], &pMuxer, L"Muxer");

        if (SUCCEEDED(hr))
        {
            hr2 = E_FAIL;

            if (SUCCEEDED(hr) && pAudioEncoder)
            {
        hr2 = ConnectFilters(m_pGraph, pAudioEncoder, pMuxer);
      }

            if (SUCCEEDED(hr2))
            {
                hr = S_OK;
            }
            else if (pAudioDecoder)
            {
                MessageBox(NULL,_T("Audio decoder to encoder connection failed"), _T("Warning"), 0);
            }

      hr2 = E_FAIL;

            if (SUCCEEDED(hr) && pVideoEncoder)
            {
                hr2 = ConnectFilters(m_pGraph, pVideoEncoder, pMuxer);
            }
            else if (pVideoDecoder)
            {
                MessageBox(NULL,_T("Video decoder to encoder connection failed"), _T("Warning"), 0);
            }

            DWORD pRegister;
            AddGraphToRot(m_pGraph, &pRegister);

            if (SUCCEEDED(hr2))
            {
                hr = S_OK;
            }
        }
    }

    //connect muxer to file writer
    if (SUCCEEDED(hr))
    {
        IFileSinkFilter* pFileSinkFilter = NULL;

        hr = AddFilterByCLSID(m_pGraph, CLSID_FileWriter, &pFileWriter, L"File Writer");

        if (SUCCEEDED(hr))
        {
            hr = pFileWriter->QueryInterface(IID_IFileSinkFilter, (void**)&pFileSinkFilter);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFileSinkFilter->SetFileName(sDstFileName, NULL);
        }

        MSDK_SAFE_RELEASE(pFileSinkFilter);

        if (SUCCEEDED(hr))
        {
            hr = ConnectFilters(m_pGraph, pMuxer, pFileWriter);
        }
    }

    m_pEncoder = pVideoEncoder;

    MSDK_SAFE_RELEASE(pSource);
    MSDK_SAFE_RELEASE(pSplitter);
    MSDK_SAFE_RELEASE(pVideoDecoder);
    MSDK_SAFE_RELEASE(pAudioDecoder);
    MSDK_SAFE_RELEASE(pVideoEncoder);
    MSDK_SAFE_RELEASE(pAudioEncoder);
    MSDK_SAFE_RELEASE(pMuxer);
    MSDK_SAFE_RELEASE(pFileWriter);
    MSDK_SAFE_RELEASE(pWNMASFReader);

    // Update our state.
    if (SUCCEEDED(hr))
    {
        m_state = STATE_STOPPED;
    }

    return hr;
};


//-----------------------------------------------------------------------------
// DShowPlayer::OpenFile
// Description: Open a new file for playback.
//-----------------------------------------------------------------------------
HRESULT DShowPlayer::OpenFile(const WCHAR* sFileName, INT nType)
{
    HRESULT hr = S_OK;

    IBaseFilter *pSource = NULL;
    IFileSourceFilter *pFileSourceAsync = NULL;

    // Create a new filter graph. (This also closes the old one, if any.)
    hr = InitializeGraph();

    // Add the source filter to the graph.
    if (SUCCEEDED(hr))
    {
        hr = AddFilterByCLSID(m_pGraph, CLSID_AsyncReader, &pSource, L"File Source Async");
    }

    if (SUCCEEDED(hr))
    {
        pSource->QueryInterface(IID_IFileSourceFilter, (void**)&pFileSourceAsync);
        hr = pFileSourceAsync->Load(sFileName, NULL);

        MSDK_SAFE_RELEASE(pFileSourceAsync);
    }

    // Try to render the streams.
    if (SUCCEEDED(hr))
    {
        if (0 == nType)
        {
            hr = RenderStreams(pSource);
        }
        else
        {
            hr = RenderStreamsS3D(pSource);
        }

    }

    // Get the seeking capabilities.
    if (SUCCEEDED(hr))
    {
        hr = m_pSeek->GetCapabilities(&m_seekCaps);
    }

    // Set the volume.
    if (SUCCEEDED(hr))
    {
        hr = UpdateVolume();
    }

    // Update our state.
    if (SUCCEEDED(hr))
    {
        m_state = STATE_STOPPED;
    }

    MSDK_SAFE_RELEASE(pSource);

    return hr;
}

//-----------------------------------------------------------------------------
// DShowPlayer::HandleGraphEvent
// Description: Respond to a graph event.
//
// The owning window should call this method when it receives the window
// message that the application specified when it called SetEventWindow.
//
// pCB: Pointer to the GraphEventCallback callback, implemented by
//      the application. This callback is invoked once for each event
//      in the queue.
//
// Caution: Do not tear down the graph from inside the callback.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::HandleGraphEvent(GraphEventCallback *pCB)
{
  if (pCB == NULL)
  {
    return E_POINTER;
  }

  if (!m_pEvent)
  {
    return E_UNEXPECTED;
  }

  long evCode = 0;
  LONG_PTR param1 = 0, param2 = 0;

  HRESULT hr = S_OK;

    // Get the events from the queue.
  while (SUCCEEDED(m_pEvent->GetEvent(&evCode, &param1, &param2, 0)))
  {
        // Invoke the callback.
    pCB->OnGraphEvent(evCode, param1, param2);

        // Free the event data.
    hr = m_pEvent->FreeEventParams(evCode, param1, param2);
    if (FAILED(hr))
    {
      break;
    }
  }

  return hr;
}


// state changes

HRESULT DShowPlayer::Play()
{
    HRESULT hr = S_OK;

  if (m_state != STATE_PAUSED && m_state != STATE_STOPPED)
  {
    return VFW_E_WRONG_STATE;
  }

    if (m_pControl == NULL || m_pSeek == NULL)
    {
        return E_UNEXPECTED;
    }

  assert(m_pGraph); // If state is correct, the graph should exist.

  hr = m_pControl->Run();

  if (SUCCEEDED(hr))
  {
    m_state = STATE_RUNNING;
  }

  return hr;
}

HRESULT DShowPlayer::Pause()
{
  if (m_state != STATE_RUNNING)
  {
    return VFW_E_WRONG_STATE;
  }

  assert(m_pGraph); // If state is correct, the graph should exist.

  HRESULT hr = m_pControl->Pause();

  if (SUCCEEDED(hr))
  {
    m_state = STATE_PAUSED;
  }

  return hr;
}


HRESULT DShowPlayer::Stop()
{
  if (m_state != STATE_RUNNING && m_state != STATE_PAUSED)
  {
    return VFW_E_WRONG_STATE;
  }

  assert(m_pGraph); // If state is correct, the graph should exist.

  HRESULT hr = m_pControl->Stop();

  if (SUCCEEDED(hr))
  {
    m_state = STATE_STOPPED;
  }

  return hr;
}


// VMR functionality



//-----------------------------------------------------------------------------
// DShowPlayer::UpdateVideoWindow
// Description: Sets the destination rectangle for the video.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::UpdateVideoWindow(const LPRECT prc)
{
  if (m_pWindowless == NULL)
  {
    return S_OK; // no-op
  }

  RECT rc;

  GetClientRect(m_hwndVideo, &rc);
    return m_pWindowless->SetVideoPosition(NULL, &rc);

}

//-----------------------------------------------------------------------------
// DShowPlayer::Repaint
// Description: Repaints the video.
//
// Call this method when the application receives WM_PAINT.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::Repaint(HDC hdc)
{
  if (m_pWindowless)
  {
    return m_pWindowless->RepaintVideo();
  }
  else
  {
    return S_OK;
  }
}


// Seeking


//-----------------------------------------------------------------------------
// DShowPlayer::CanSeek
// Description: Returns TRUE if the current file is seekable.
//-----------------------------------------------------------------------------

BOOL DShowPlayer::CanSeek() const
{
  const DWORD caps = AM_SEEKING_CanSeekAbsolute;
  return ((m_seekCaps & caps) == caps);
}


//-----------------------------------------------------------------------------
// DShowPlayer::SetPosition
// Description: Seeks to a new position.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::SetPosition(REFERENCE_TIME pos)
{
  if (m_pControl == NULL || m_pSeek == NULL || m_pEncoder)
  {
    return E_UNEXPECTED;
  }

  HRESULT hr = S_OK;

  hr = m_pSeek->SetPositions(&pos, AM_SEEKING_AbsolutePositioning,
    NULL, AM_SEEKING_NoPositioning);

  if (SUCCEEDED(hr))
  {
    // If playback is stopped, we need to put the graph into the paused
    // state to update the video renderer with the new frame, and then stop
    // the graph again. The IMediaControl::StopWhenReady does this.
    if (m_state == STATE_STOPPED)
    {
      hr = m_pControl->StopWhenReady();
    }
  }

  return hr;
}

//-----------------------------------------------------------------------------
// DShowPlayer::GetDuration
// Description: Gets the duration of the current file.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::GetDuration(LONGLONG *pDuration)
{
  if (m_pSeek == NULL)
  {
    return E_UNEXPECTED;
  }

  return m_pSeek->GetDuration(pDuration);
}

//-----------------------------------------------------------------------------
// DShowPlayer::GetCurrentPosition
// Description: Gets the current playback position.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::GetCurrentPosition(LONGLONG *pTimeNow)
{
  if (m_pSeek == NULL)
  {
    return E_UNEXPECTED;
  }

  return m_pSeek->GetCurrentPosition(pTimeNow);
}


// Audio

//-----------------------------------------------------------------------------
// DShowPlayer::Mute
// Description: Mutes or unmutes the audio.
//-----------------------------------------------------------------------------

HRESULT  DShowPlayer::Mute(BOOL bMute)
{
  m_bMute = bMute;
  return UpdateVolume();
}

//-----------------------------------------------------------------------------
// DShowPlayer::SetVolume
// Description: Sets the volume.
//-----------------------------------------------------------------------------

HRESULT  DShowPlayer::SetVolume(long lVolume)
{
  m_lVolume = lVolume;
  return UpdateVolume();
}


//-----------------------------------------------------------------------------
// DShowPlayer::UpdateVolume
// Description: Update the volume after a call to Mute() or SetVolume().
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::UpdateVolume()
{
  HRESULT hr = S_OK;

  if (m_bAudioStream && m_pAudio)
  {
        // If the audio is muted, set the minimum volume.
    if (m_bMute)
    {
      hr = m_pAudio->put_Volume(MIN_VOLUME);
    }
    else
    {
      // Restore previous volume setting
      hr = m_pAudio->put_Volume(m_lVolume);
    }
  }

  return hr;
}

// Graph building


//-----------------------------------------------------------------------------
// DShowPlayer::InitializeGraph
// Description: Create a new filter graph. (Tears down the old graph.)
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::InitializeGraph()
{
  HRESULT hr = S_OK;

  TearDownGraph();

  // Create the Filter Graph Manager.
  hr = CoCreateInstance(
    CLSID_FilterGraph,
    NULL,
    CLSCTX_INPROC_SERVER,
    IID_IGraphBuilder,
    (void**)&m_pGraph
    );

  // Query for graph interfaces.
  if (SUCCEEDED(hr))
  {
    hr = m_pGraph->QueryInterface(IID_IMediaControl, (void**)&m_pControl);
  }

  if (SUCCEEDED(hr))
  {
    hr = m_pGraph->QueryInterface(IID_IMediaEventEx, (void**)&m_pEvent);
  }

  if (SUCCEEDED(hr))
  {
    hr = m_pGraph->QueryInterface(IID_IMediaSeeking, (void**)&m_pSeek);
  }

  if (SUCCEEDED(hr))
  {
    hr = m_pGraph->QueryInterface(IID_IBasicAudio, (void**)&m_pAudio);
  }

  // Set up event notification.
  if (SUCCEEDED(hr))
  {
    hr = m_pEvent->SetNotifyWindow((OAHWND)m_hwndEvent, m_EventMsg, NULL);
  }

  return hr;
}

//-----------------------------------------------------------------------------
// DShowPlayer::TearDownGraph
// Description: Tear down the filter graph and release resources.
//-----------------------------------------------------------------------------

void DShowPlayer::TearDownGraph()
{
  // Stop sending event messages
  if (m_pEvent)
  {
    m_pEvent->SetNotifyWindow((OAHWND)NULL, NULL, NULL);
  }

  MSDK_SAFE_RELEASE(m_pGraph);
  MSDK_SAFE_RELEASE(m_pControl);
  MSDK_SAFE_RELEASE(m_pEvent);
  MSDK_SAFE_RELEASE(m_pSeek);
  MSDK_SAFE_RELEASE(m_pWindowless);
  MSDK_SAFE_RELEASE(m_pAudio);

    m_pEncoder.Release();
    m_pVMR.Release();

    MSDK_SAFE_RELEASE(m_pCustomPresenter);


  m_state = STATE_CLOSED;
  m_seekCaps = 0;

    m_bAudioStream = FALSE;
}

HRESULT DShowPlayer::ConnectFilterToFilter(IBaseFilter *pSrc, IBaseFilter** pDst, FilterType nType)
{
    HRESULT         hr    = E_FAIL;
    IEnumPins*      pEnum = NULL;
    IPin*           pPin  = NULL;
    PIN_DIRECTION   dir;

    GUID  guidFilters[10];
    INT   nArraySize = 0;
    TCHAR strFilterName[64];

    if ((m_pGraph == NULL) || (pSrc == NULL))
    {
        return E_POINTER;
    }

    if (*pDst != NULL)
    {
        return E_INVALIDARG;
    }

    memset(guidFilters, 0, sizeof(GUID) * 10);

    switch (nType)
    {
    case SPLITTER:
        nArraySize = ARRAYSIZE(guidSplitters);
        _tcscpy_s(strFilterName, L"Splitter");
        memcpy_s(guidFilters, sizeof(GUID) * 10, guidSplitters, sizeof(GUID) * nArraySize);
        break;
    case VIDEO_DECODER:
        nArraySize = ARRAYSIZE(guidVideoDecoders);
        _tcscpy_s(strFilterName, L"Video Decoder");
        memcpy_s(guidFilters, sizeof(GUID) * 10, guidVideoDecoders, sizeof(GUID) * nArraySize);
        break;
    case VIDEO_DECODER3D:
        nArraySize = ARRAYSIZE(guidVideoDecodersS3D);
        _tcscpy_s(strFilterName, L"Video Decoder");
        memcpy_s(guidFilters, sizeof(GUID) * 10, guidVideoDecodersS3D, sizeof(GUID) * nArraySize);
        break;
    case AUDIO_DECODER:
        nArraySize = ARRAYSIZE(guidAudioDecoders);
        _tcscpy_s(strFilterName, L"Audio Decoder");
        memcpy_s(guidFilters, sizeof(GUID) * 10, guidAudioDecoders, sizeof(GUID) * nArraySize);
        break;
    }

    for (int i = 0; i < nArraySize; i++)
    {
        hr = AddFilterByCLSID(m_pGraph, guidFilters[i], pDst, strFilterName);

        if (SUCCEEDED(hr))
        {
            // Enumerate the pins on the source filter.
            hr = pSrc->EnumPins(&pEnum);

            if (SUCCEEDED(hr))
            {
                while (S_OK == pEnum->Next(1, &pPin, NULL))
                {
                    hr = pPin->QueryDirection(&dir);

                    if (FAILED(hr) || dir == PINDIR_INPUT)
                    {
                        MSDK_SAFE_RELEASE(pPin);
                        continue;
                    }

                    hr = ConnectFilters(m_pGraph, pPin, *pDst);

                    MSDK_SAFE_RELEASE(pPin);

                    if (SUCCEEDED(hr))
                    {
                        break;
                    }
                }

                MSDK_SAFE_RELEASE(pEnum);
            }
        }

        if (SUCCEEDED(hr))
        {
            break;
        }

        if (NULL != *pDst)
        {
            m_pGraph->RemoveFilter(*pDst);
            MSDK_SAFE_RELEASE((*pDst));
        }
    }

    return hr;
};

HRESULT DShowPlayer::ConnectToDMOAudio(IBaseFilter *pSrc, IBaseFilter** pDst)
{
    HRESULT hr = S_OK;
    IDMOWrapperFilter *pWraperFilter = NULL;

    // Create the DMO Wrapper filter.
    hr = CoCreateInstance(CLSID_DMOWrapperFilter, NULL, CLSCTX_INPROC, IID_IBaseFilter, (void **)pDst);

    if (SUCCEEDED(hr))
    {
        hr = (*pDst)->QueryInterface(IID_IDMOWrapperFilter, (void **)&pWraperFilter);
    }

    if(SUCCEEDED(hr))
    {
        // Initialize the filter.
        hr = pWraperFilter->Init(CLSID_CWMADecMediaObject, DMOCATEGORY_AUDIO_DECODER);
    }

    if(SUCCEEDED(hr))
    {
        hr = m_pGraph->AddFilter(*pDst, L"Audio Decoder");
    }

    if (SUCCEEDED(hr))
    {
        hr = ConnectFilters(m_pGraph, pSrc, *pDst);
    }

    if (FAILED(hr) && NULL != *pDst)
    {
        m_pGraph->RemoveFilter(*pDst);
        MSDK_SAFE_RELEASE((*pDst));
    }

    MSDK_SAFE_RELEASE(pWraperFilter);

    return hr;
};

//-----------------------------------------------------------------------------
// DShowPlayer::RenderStreams
// Description: Render the streams from a source filter.
//-----------------------------------------------------------------------------

HRESULT  DShowPlayer::RenderStreams(IBaseFilter *pSource)
{
  HRESULT hr = S_OK;

  BOOL bRenderedAnyPin = FALSE;

  IBaseFilter *pAudioRenderer = NULL;
    IBaseFilter* pSplitter     = NULL;
    IBaseFilter* pVideoDecoder = NULL;
    IBaseFilter* pAudioDecoder = NULL;

    IFileSourceFilter* pWNMASFReader = NULL;

  // Add the VMR-9 to the graph.
  if (SUCCEEDED(hr))
  {
    hr = AddFilterByCLSID(m_pGraph, CLSID_EnhancedVideoRenderer, &m_pVMR, L"Video Rendered");
  }

  // Set windowless mode on the VMR. This must be done before the VMR is connected.
  if (SUCCEEDED(hr))
  {
    hr = InitWindowlessVMR(m_pVMR, m_hwndVideo, &m_pWindowless);
  }

  // Add the DSound Renderer to the graph.
  if (SUCCEEDED(hr))
  {
    hr = AddFilterByCLSID(m_pGraph, CLSID_DSoundRender, &pAudioRenderer, L"Audio Renderer");
  }

    //connect source to splitter
  if (SUCCEEDED(hr))
  {
        hr = ConnectFilterToFilter(pSource, &pSplitter, SPLITTER);
    }

    //if connect to splitter failed suppose that file is wmv
    if (FAILED(hr))
    {
        LPOLESTR strFileName = NULL;

        hr = pSource->QueryInterface(IID_IFileSourceFilter, (void**)&pWNMASFReader);

        if (SUCCEEDED(hr))
        {
            hr = pWNMASFReader->GetCurFile(&strFileName, NULL);
            MSDK_SAFE_RELEASE(pWNMASFReader);
        }

        if (SUCCEEDED(hr))
        {
            hr = m_pGraph->RemoveFilter(pSource);
        }

        if (SUCCEEDED(hr))
        {
            hr = AddFilterByCLSID(m_pGraph, CLSID_WMAsfReader, &pSplitter, L"ASF Reader");
        }

        if (SUCCEEDED(hr))
        {
            hr = pSplitter->QueryInterface(IID_IFileSourceFilter, (void**)&pWNMASFReader);
        }

        if (SUCCEEDED(hr))
        {
            hr = pWNMASFReader->Load(strFileName, NULL);
        }
    }

    //connect splitter to video and to render
    if (SUCCEEDED(hr))
    {
        //video
        hr = ConnectFilterToFilter(pSplitter, &pVideoDecoder, VIDEO_DECODER);

        if (SUCCEEDED(hr))
        {
            hr=ConnectFilters(m_pGraph, pVideoDecoder, m_pVMR);
        }

        if (SUCCEEDED(hr))
        {
            bRenderedAnyPin = true;
        }
        else
        {
            MSDK_SAFE_RELEASE(pVideoDecoder);
            return hr;
        }

        //audio
        hr = ConnectFilterToFilter(pSplitter, &pAudioDecoder, AUDIO_DECODER);

        //try to connect with WMAudio DMO
        if (FAILED(hr))
        {
            hr = ConnectToDMOAudio(pSplitter, &pAudioDecoder);
        }

        if (SUCCEEDED(hr))
        {
            hr=ConnectFilters(m_pGraph, pAudioDecoder, pAudioRenderer);
        }

        if (SUCCEEDED(hr))
        {
            bRenderedAnyPin = true;
        }
        else
        {
            MSDK_SAFE_RELEASE(pAudioDecoder);
        }

        if (bRenderedAnyPin)
        {
            hr = S_OK;
        }
    }

  // Remove un-used renderers.
    // Try to remove the VMR.
  if (SUCCEEDED(hr) && m_pVMR)
  {
      BOOL bRemoved = FALSE;
    hr = RemoveUnconnectedRenderer(m_pGraph, m_pVMR, &bRemoved);

    // If we removed the VMR, then we also need to release our
    // pointer to the VMR's windowless control interface.
    if (bRemoved)
    {
      MSDK_SAFE_RELEASE(m_pWindowless);
    }
  }

    // Try to remove the audio renderer.
  if (SUCCEEDED(hr) && pAudioRenderer)
  {
      BOOL bRemoved = FALSE;
    hr = RemoveUnconnectedRenderer(m_pGraph, pAudioRenderer, &bRemoved);

        if (bRemoved)
        {
            m_bAudioStream = FALSE;
        }
        else
        {
            m_bAudioStream = TRUE;
        }
  }

    MSDK_SAFE_RELEASE(pSplitter);
    MSDK_SAFE_RELEASE(pVideoDecoder);
    MSDK_SAFE_RELEASE(pAudioDecoder);
  MSDK_SAFE_RELEASE(pAudioRenderer);

  // If we succeeded to this point, make sure we rendered at least one
  // stream.
  if (SUCCEEDED(hr))
  {
    if (!bRenderedAnyPin)
    {
      hr = VFW_E_CANNOT_RENDER;
    }
  }

  return hr;
}


HRESULT  DShowPlayer::RenderStreamsS3D(IBaseFilter *pSource)
{
    HRESULT hr = S_OK;

    BOOL bRenderedAnyPin = FALSE;

    IBaseFilter *pAudioRenderer = NULL;
    IBaseFilter* pSplitter     = NULL;
    IBaseFilter* pVideoDecoder = NULL;
    IBaseFilter* pAudioDecoder = NULL;

    CComPtr<IMFGetService>    pGetService = NULL;
    CComPtr<IMFVideoRenderer> pVideoRenderer = NULL;

    // Add the VMR-9 to the graph.
    if (SUCCEEDED(hr))
    {
        hr = AddFilterByCLSID(m_pGraph, CLSID_EnhancedVideoRenderer, &m_pVMR, L"Video Rendered");
    }

    // Create custom presenter for EVR
    if (SUCCEEDED(hr))
    {
        hr = CoCreateInstance(CLSID_CustomEVRPresenter, NULL, CLSCTX_INPROC_SERVER, __uuidof(IMFVideoPresenter), (void **)&m_pCustomPresenter);
    }

    if (SUCCEEDED(hr))
    {
        hr = m_pVMR->QueryInterface(__uuidof(IMFGetService), (void**)&pGetService);
    }

    if (SUCCEEDED(hr))
    {
        hr = m_pVMR->QueryInterface(__uuidof(IMFVideoRenderer),(void**)&pVideoRenderer);
    }

    // Set custom presenter on EVR
    if (SUCCEEDED(hr))
    {
        hr = pVideoRenderer->InitializeRenderer(NULL, m_pCustomPresenter);
    }

    // Set windowless mode on the VMR. This must be done before the VMR is connected.
    if (SUCCEEDED(hr))
    {
        hr = InitWindowlessVMR(m_pVMR, m_hwndVideo, &m_pWindowless);
    }

    // Add the DSound Renderer to the graph.
    if (SUCCEEDED(hr))
    {
        hr = AddFilterByCLSID(m_pGraph, CLSID_DSoundRender, &pAudioRenderer, L"Audio Renderer");
    }

    //connect source to splitter
    if (SUCCEEDED(hr))
    {
        hr = ConnectFilterToFilter(pSource, &pSplitter, SPLITTER);
    }

    //connect splitter to video and to render
    if (SUCCEEDED(hr))
    {
        //video
        hr = ConnectFilterToFilter(pSplitter, &pVideoDecoder, VIDEO_DECODER3D);
        if (FAILED(hr))
        {
            return hr;
        }

        if (SUCCEEDED(hr))
        {
            hr=ConnectFilters(m_pGraph, pVideoDecoder, m_pVMR);
        }

        if (SUCCEEDED(hr))
        {
            bRenderedAnyPin = true;
        }
        else
        {
            MSDK_SAFE_RELEASE(pVideoDecoder);
        }

        //audio
        hr = ConnectFilterToFilter(pSplitter, &pAudioDecoder, AUDIO_DECODER);

        //try to connect with WMAudio DMO
        if (FAILED(hr))
        {
            hr = ConnectToDMOAudio(pSplitter, &pAudioDecoder);
        }

        if (SUCCEEDED(hr))
        {
            hr=ConnectFilters(m_pGraph, pAudioDecoder, pAudioRenderer);
        }

        if (SUCCEEDED(hr))
        {
            bRenderedAnyPin = true;
        }
        else
        {
            MSDK_SAFE_RELEASE(pAudioDecoder);
        }

        if (bRenderedAnyPin)
        {
            hr = S_OK;
        }
    }

    // Remove un-used renderers.
    // Try to remove the VMR.
    if (SUCCEEDED(hr) && m_pVMR)
    {
        BOOL bRemoved = FALSE;
        hr = RemoveUnconnectedRenderer(m_pGraph, m_pVMR, &bRemoved);

        // If we removed the VMR, then we also need to release our
        // pointer to the VMR's windowless control interface.
        if (bRemoved)
        {
            MSDK_SAFE_RELEASE(m_pWindowless);
        }
    }

    // Try to remove the audio renderer.
    if (SUCCEEDED(hr) && pAudioRenderer)
    {
        BOOL bRemoved = FALSE;
        hr = RemoveUnconnectedRenderer(m_pGraph, pAudioRenderer, &bRemoved);

        if (bRemoved)
        {
            m_bAudioStream = FALSE;
        }
        else
        {
            m_bAudioStream = TRUE;
        }
    }

    MSDK_SAFE_RELEASE(pSplitter);
    MSDK_SAFE_RELEASE(pVideoDecoder);
    MSDK_SAFE_RELEASE(pAudioDecoder);
    MSDK_SAFE_RELEASE(pAudioRenderer);

    // If we succeeded to this point, make sure we rendered at least one
    // stream.
    if (SUCCEEDED(hr))
    {
        if (!bRenderedAnyPin)
        {
            hr = VFW_E_CANNOT_RENDER;
        }
    }

    return hr;
}

//-----------------------------------------------------------------------------
// DShowPlayer::RemoveUnconnectedRenderer
// Description: Remove a renderer filter from the graph if the filter is
//              not connected.
//-----------------------------------------------------------------------------

HRESULT RemoveUnconnectedRenderer(IGraphBuilder *pGraph, IBaseFilter *pRenderer, BOOL *pbRemoved)
{
  IPin *pPin = NULL;

  BOOL bRemoved = FALSE;

  // Look for a connected input pin on the renderer.

  HRESULT hr = FindConnectedPin(pRenderer, PINDIR_INPUT, &pPin);
  MSDK_SAFE_RELEASE(pPin);

  // If this function succeeds, the renderer is connected, so we don't remove it.
  // If it fails, it means the renderer is not connected to anything, so
  // we remove it.

  if (FAILED(hr))
  {
    hr = pGraph->RemoveFilter(pRenderer);
    bRemoved = TRUE;
  }

  if (SUCCEEDED(hr))
  {
    *pbRemoved = bRemoved;
  }

  return hr;
}

//-----------------------------------------------------------------------------
// DShowPlayer::InitWindowlessVMR
// Description: Initialize the VMR-9 for windowless mode.
//-----------------------------------------------------------------------------

HRESULT InitWindowlessVMR(
    IBaseFilter *pVMR,        // Pointer to the render
  HWND hwnd,            // Clipping window
  IMFVideoDisplayControl** ppWC  // Receives a pointer to the render config.
    )
{
    HRESULT                         hr = S_OK;
    CComPtr<IMFGetService>          pGetService = NULL;
    CComPtr<IMFVideoDisplayControl> pConfig = NULL;

    hr = pVMR->QueryInterface(__uuidof(IMFGetService), (void**)&pGetService);

    if (SUCCEEDED(hr))
    {
        hr = pGetService->GetService(MR_VIDEO_RENDER_SERVICE, IID_IMFVideoDisplayControl, (void**)&pConfig);
    }

    if (SUCCEEDED(hr))
    {
        hr = pConfig->SetVideoWindow(hwnd);
    }

    if (SUCCEEDED(hr))
    {
        pConfig->SetAspectRatioMode(MFVideoARMode_PreservePicture);
    }

  // Return the IVMRWindowlessControl pointer to the caller.
  if (SUCCEEDED(hr))
  {
    *ppWC = pConfig;
    (*ppWC)->AddRef();
  }

  return hr;
}
