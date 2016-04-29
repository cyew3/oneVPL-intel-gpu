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
// DShowPlayer.h: Implements DirectShow playback functionality.
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

#include <dshow.h>
#include "D3d9.h"
#include "evr.h"
#include "Vmr9.h"
#include "atlbase.h"
#include "Dmodshow.h"
#include "dmoreg.h"

#include <dshowasf.h>
#include <uuids.h>

const long MIN_VOLUME = -10000;
const long MAX_VOLUME = 0;

// {CCCE52FD-02CB-482c-AC81-1E55EF1D61EE}
static const GUID CLSID_H264DecFilter =
{ 0xccce52fd, 0x2cb, 0x482c, { 0xac, 0x81, 0x1e, 0x55, 0xef, 0x1d, 0x61, 0xee } };

// {936E6340-19A8-4a58-92AE-695FD64B9418}
static const GUID CLSID_MPEG2DecFilter =
{ 0x936e6340, 0x19a8, 0x4a58, { 0x92, 0xae, 0x69, 0x5f, 0xd6, 0x4b, 0x94, 0x18 } };

// {F58D5C1C-8EC7-4e74-B3A9-CED73B25F4A1}
static const GUID CLSID_VC1DecFilter =
{ 0xf58d5c1c, 0x8ec7, 0x4e74, { 0xb3, 0xa9, 0xce, 0xd7, 0x3b, 0x25, 0xf4, 0xa1 } };

// {71183C45-F4FA-4b10-9E04-F9040CB19139}
static const GUID CLSID_H264EncFilter =
{ 0x71183c45, 0xf4fa, 0x4b10, { 0x9e, 0x4, 0xf9, 0x4, 0xc, 0xb1, 0x91, 0x39 } };

// {F0EAA393-2ACD-4cbe-8F4D-990DEB6C67E6}
static const GUID CLSID_MPEG2EncFilter =
{ 0xf0eaa393, 0x2acd, 0x4cbe, { 0x8f, 0x4d, 0x99, 0xd, 0xeb, 0x6c, 0x67, 0xe6 } };

// {281D4741-787E-4a2d-B518-69C4CB1D7227}
static const GUID CLSID_VC1EncFilter =
{ 0x281d4741, 0x787e, 0x4a2d, { 0xb5, 0x18, 0x69, 0xc4, 0xcb, 0x1d, 0x72, 0x27 } };

// {2eeb4adf-4578-4d10-bca7-bb955f56320a}
static const CLSID CLSID_CWMADecMediaObject =
{ 0x2eeb4adf, 0x4578, 0x4d10, { 0xbc, 0xa7, 0xbb, 0x95, 0x5f, 0x56, 0x32, 0x0a } };

// {41E5E4D6-7635-4c43-8A06-DD856470856F}
static const GUID CLSID_MPEG2SplitterFilter =
{ 0x41e5e4d6, 0x7635, 0x4c43, { 0x8a, 0x6, 0xdd, 0x85, 0x64, 0x70, 0x85, 0x6f } };

// {A2A6B846-D118-4300-AE07-F31860887BC2}
static const GUID CLSID_MP4SplitterFilter =
{ 0xa2a6b846, 0xd118, 0x4300, { 0xae, 0x7, 0xf3, 0x18, 0x60, 0x88, 0x7b, 0xc2 } };

//AUDIO FILTERS

// {E7FACCFD-9148-4871-B302-60D7A1FC6270}
static const GUID CLSID_AC3DecFilter =
{ 0xe7faccfd, 0x9148, 0x4871, { 0xb3, 0x2, 0x60, 0xd7, 0xa1, 0xfc, 0x62, 0x70 } };

// {06079E43-C107-4b50-8450-3C09FF5E832E}
static const GUID CLSID_MP3DecFilter =
{ 0x6079e43, 0xc107, 0x4b50, { 0x84, 0x50, 0x3c, 0x9, 0xff, 0x5e, 0x83, 0x2e } };

// {8DA364BE-DF1D-43f9-9A86-CC06F53C082C}
static const GUID CLSID_AACDecFilter =
{ 0x8da364be, 0xdf1d, 0x43f9, { 0x9a, 0x86, 0xcc, 0x6, 0xf5, 0x3c, 0x8, 0x2c } };

//aac encoder filter GIUDS
const GUID CLSID_AACEncFilter =
{ 0xe51ef49d, 0xddb0, 0x4874, { 0xa8, 0x73, 0xc5, 0x10, 0x1, 0x71, 0x14, 0x6f } };

// {CECE2B60-4954-41ac-8971-ECD874A4C368}
const GUID CLSID_MP3EncFilter =
{ 0xcece2b60, 0x4954, 0x41ac, { 0x89, 0x71, 0xec, 0xd8, 0x74, 0xa4, 0xc3, 0x68 } };

// MUXER FILTERS

// {CB488050-23B8-411d-B861-D00BA44B8D02}
static const GUID CLSID_MP4MuxerFilter =
{ 0xcb488050, 0x23b8, 0x411d, { 0xb8, 0x61, 0xd0, 0xb, 0xa4, 0x4b, 0x8d, 0x2 } };

// {AF76B26C-ECDE-4515-BB41-C149BBC362CE}
static const GUID CLSID_MPEG2MuxerFilter =
{ 0xaf76b26c, 0xecde, 0x4515, { 0xbb, 0x41, 0xc1, 0x49, 0xbb, 0xc3, 0x62, 0xce } };

// {834E3A61-4576-46fe-892A-602F414F03EE}
static const GUID CLSID_MVCDecFilter =
{ 0x834e3a61, 0x4576, 0x46fe, { 0x89, 0x2a, 0x60, 0x2f, 0x41, 0x4f, 0x3, 0xee } };

//custom presenter
// {29FAB022-F7CC-4819-B2B8-D9B6BCFB6698}
static const GUID CLSID_CustomEVRPresenter =
{ 0x29fab022, 0xf7cc, 0x4819, { 0xb2, 0xb8, 0xd9, 0xb6, 0xbc, 0xfb, 0x66, 0x98 } };

static const GUID guidSplitters[]           = {CLSID_MP4SplitterFilter, CLSID_MPEG2SplitterFilter};
static const GUID guidMuxers[]              = {CLSID_MP4MuxerFilter, CLSID_MP4MuxerFilter, CLSID_MPEG2MuxerFilter, CLSID_MPEG2MuxerFilter};
static const GUID guidVideoDecoders[]       = {CLSID_VC1DecFilter, CLSID_H264DecFilter, CLSID_MPEG2DecFilter};
static const GUID guidVideoDecodersS3D[]    = {CLSID_MVCDecFilter};
static const GUID guidAudioDecoders[]       = {CLSID_AACDecFilter, CLSID_MP3DecFilter};
static const GUID guidAudioEncoders[]       = {CLSID_MP3EncFilter, CLSID_AACEncFilter, CLSID_MP3EncFilter, CLSID_AACEncFilter};
static const GUID guidVideoEncoders[]       = {CLSID_H264EncFilter, CLSID_H264EncFilter, CLSID_MPEG2EncFilter, CLSID_H264EncFilter};

enum FilterType
{
  SPLITTER,
  VIDEO_DECODER,
  VIDEO_DECODER3D,
  AUDIO_DECODER
};

enum PlaybackState
{
  STATE_RUNNING,
  STATE_PAUSED,
  STATE_STOPPED,
  STATE_CLOSED
};

struct GraphEventCallback
{
  virtual void OnGraphEvent(long eventCode, LONG_PTR param1, LONG_PTR param2) = 0;
};


class DShowPlayer
{

public:

  DShowPlayer(HWND hwndVideo);
  ~DShowPlayer();

  HRESULT SetEventWindow(HWND hwnd, UINT msg);

  PlaybackState State() const { return m_state; }

  HRESULT OpenFile(const WCHAR* sFileName, INT nType);
  HRESULT TranscodeFile(const WCHAR* sSrcFileName, const WCHAR* sDstFileName, INT nType);

  // Streaming
  HRESULT Play();
  HRESULT Pause();
  HRESULT Stop();

  // VMR functionality
  BOOL    HasVideo() const { return m_pWindowless != NULL; }
  HRESULT UpdateVideoWindow(const LPRECT prc);
  HRESULT Repaint(HDC hdc);

  // events
  HRESULT HandleGraphEvent(GraphEventCallback *pCB);

  // seeking
  BOOL  CanSeek() const;
  HRESULT SetPosition(REFERENCE_TIME pos);
  HRESULT GetDuration(LONGLONG *pDuration);
  HRESULT GetCurrentPosition(LONGLONG *pTimeNow);

  // Audio
  HRESULT  Mute(BOOL bMute);
  BOOL  IsMuted() const { return m_bMute; }
  HRESULT  SetVolume(long lVolume);
  long  GetVolume() const { return m_lVolume; }

  CComPtr<IBaseFilter> GetEncoder() {return m_pEncoder;}
  CComPtr<IBaseFilter> GetRender() {return m_pVMR;}

private:
  HRESULT InitializeGraph();
  void  TearDownGraph();
  HRESULT  RenderStreams(IBaseFilter *pSource);
  HRESULT  RenderStreamsS3D(IBaseFilter *pSource);
  HRESULT UpdateVolume();

  HRESULT ConnectFilterToFilter(IBaseFilter *pSrc, IBaseFilter** pDst, FilterType nType);
  HRESULT ConnectToDMOAudio(IBaseFilter *pSrc, IBaseFilter** pDst);

  PlaybackState  m_state;

  HWND      m_hwndVideo;  // Video clipping window
  HWND      m_hwndEvent;  // Window to receive events
  UINT      m_EventMsg;    // Windows message for graph events

  DWORD      m_seekCaps;    // Caps bits for IMediaSeeking

  // Audio
  BOOL            m_bAudioStream; // Is there an audio stream?
  long      m_lVolume;    // Current volume (unless muted)
  BOOL      m_bMute;    // Volume muted?

  IGraphBuilder  *m_pGraph;
  IMediaControl  *m_pControl;
  IMediaEventEx  *m_pEvent;
  IMediaSeeking  *m_pSeek;
  IBasicAudio    *m_pAudio;

  IMFVideoDisplayControl  *m_pWindowless;

  CComPtr<IBaseFilter>    m_pEncoder;
  CComPtr<IBaseFilter>    m_pVMR;

  IMFVideoPresenter       *m_pCustomPresenter;
};