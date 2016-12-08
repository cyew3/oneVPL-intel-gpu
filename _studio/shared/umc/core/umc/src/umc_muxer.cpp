//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#include "vm_debug.h"
#include "umc_muxer.h"
#include <ipps.h>

using namespace UMC;

// this line to turn off excessive output, comment it to get full debug output
//#define VM_DEBUG_PROGRESS 0

/******************************************************************************/

MuxerParams::MuxerParams()
{
  m_SystemType = UNDEF_STREAM;
  m_lFlags = 0;
  m_nNumberOfTracks = 0;
  pTrackParams = NULL;
  m_lpDataWriter = NULL;
}

MuxerParams::~MuxerParams()
{
    Close();
}

void MuxerParams::Close()
{
    for (Ipp32s i = 0; i < m_nNumberOfTracks; i++)
    {
        if (pTrackParams[i].type == VIDEO_TRACK)
        {
            delete pTrackParams[i].info.video;
            pTrackParams[i].info.video = 0;
        }
        else if (pTrackParams[i].type == AUDIO_TRACK)
        {
            delete pTrackParams[i].info.audio;
            pTrackParams[i].info.audio = 0;
        }
    }

    delete [] pTrackParams;
    pTrackParams = 0;
}

MuxerParams & MuxerParams::operator=(const MuxerParams & p)
{
    //  check for self-assignment
    if (this == &p)
    {
        return *this;
    }

    TrackParams * new_pTrackParams = new TrackParams[p.m_nNumberOfTracks];

    for (Ipp32s i = 0; i < p.m_nNumberOfTracks; i++)
    {
        pTrackParams[i].bufferParams = p.pTrackParams[i].bufferParams;
        pTrackParams[i].type         = p.pTrackParams[i].type;
        pTrackParams[i].info.undef   = 0;

        if (p.pTrackParams[i].type == VIDEO_TRACK && p.pTrackParams[i].info.video != 0)
            pTrackParams[i].info.video = new VideoStreamInfo(*p.pTrackParams[i].info.video);
        else if (p.pTrackParams[i].type == AUDIO_TRACK && p.pTrackParams[i].info.audio != 0)
            pTrackParams[i].info.audio = new AudioStreamInfo(*p.pTrackParams[i].info.audio);
    }

    Close();

    pTrackParams = new_pTrackParams;
    m_SystemType = p.m_SystemType;
    m_lFlags = p.m_lFlags;
    m_nNumberOfTracks = p.m_nNumberOfTracks;
    m_lpDataWriter = p.m_lpDataWriter;

    return *this;
}

/******************************************************************************/

Muxer::Muxer()
{
  m_pParams = NULL;
  m_ppBuffers = NULL;
  m_uiTotalNumStreams = 0;
  m_pTrackParams = NULL;
}

Muxer::~Muxer()
{
  Muxer::Close();
}

Status Muxer::CopyMuxerParams(MuxerParams *lpInit)
{
  Ipp32u i;

  UMC_CHECK_PTR(lpInit);
  if (!m_pParams) {
    UMC_NEW(m_pParams, MuxerParams);
  }

  m_uiTotalNumStreams = lpInit->m_nNumberOfTracks;
  UMC_CHECK(lpInit->m_nNumberOfTracks >= 0, UMC_ERR_INVALID_PARAMS);

  *m_pParams = *lpInit; // via operator= !!!
  m_pTrackParams = m_pParams->pTrackParams; // copy of pointer, don't delete!

  // check MediaBufferParams
  for (i = 0; i < m_uiTotalNumStreams; i++) {
    UMC_CHECK_PTR(lpInit->pTrackParams[i].info.undef);
    if (!m_pTrackParams[i].bufferParams.m_prefInputBufferSize) {
      Ipp32s bitrate = 4000000;
      if (m_pTrackParams[i].type == VIDEO_TRACK) {
        bitrate = m_pTrackParams[i].info.video->bitrate;
        if (0 == bitrate) {
          bitrate = 4000000;
        }
      } else if (m_pTrackParams[i].type == AUDIO_TRACK) {
        bitrate = m_pTrackParams[i].info.audio->bitrate;
        if (0 == bitrate) {
          bitrate = 100000;
        }
      } else {
        return UMC_ERR_INVALID_PARAMS;
      }
      m_pTrackParams[i].bufferParams.m_prefInputBufferSize = bitrate >> 3; /* 1 sec in bytes */
    }
    if (!m_pTrackParams[i].bufferParams.m_prefOutputBufferSize) {
      m_pTrackParams[i].bufferParams.m_prefOutputBufferSize = m_pTrackParams[i].bufferParams.m_prefInputBufferSize;
    }
    if (!m_pTrackParams[i].bufferParams.m_numberOfFrames) {
      m_pTrackParams[i].bufferParams.m_numberOfFrames = 5;
    }
  }

  // Alloc pointers to MediaBuffer (and set to NULL)
  m_ppBuffers = new MediaBuffer*[m_uiTotalNumStreams];
  memset(m_ppBuffers, 0, sizeof(MediaBuffer*)*m_uiTotalNumStreams);

  return UMC_OK;
}

Status Muxer::Close()
{
  if (m_ppBuffers) {
    Ipp32u i;
    for (i = 0; i < m_uiTotalNumStreams; i++) {
      UMC_DELETE(m_ppBuffers[i]);
    }
    delete[] m_ppBuffers;
  }

  UMC_DELETE(m_pParams);

  return UMC_OK;
}

Ipp32s Muxer::GetTrackIndex(MuxerTrackType type, Ipp32s index)
{
  Ipp32u i;
  for (i = 0; i < m_uiTotalNumStreams; i++) {
    if (m_pTrackParams[i].type == type) {
      if (index <= 0) {
        vm_debug_trace2(VM_DEBUG_PROGRESS, VM_STRING("GetTrackIndex: type = %d, index = %d\n"), (Ipp32s)type, i);
        return (Ipp32s)i;
      }
      index--;
    }
  }
  return -1;
}

Status Muxer::LockBuffer(MediaData *lpData, Ipp32s iTrack)
{
  UMC_CHECK_PTR(lpData);
  UMC_CHECK(iTrack >= 0, UMC_ERR_INVALID_PARAMS);
  UMC_CHECK((Ipp32u)iTrack < m_uiTotalNumStreams, UMC_ERR_INVALID_PARAMS);
  UMC_CHECK(m_ppBuffers, UMC_ERR_NOT_INITIALIZED);

  UMC_CALL(m_ppBuffers[iTrack]->LockInputBuffer(lpData));
  return UMC_OK;
} //Status Muxer::LockBuffer(MediaData *lpData, Ipp32u iTrack)

Status Muxer::UnlockBuffer(MediaData *lpData, Ipp32s iTrack)
{
  UMC_CHECK_PTR(lpData);
  UMC_CHECK(iTrack >= 0, UMC_ERR_INVALID_PARAMS);
  UMC_CHECK((Ipp32u)iTrack < m_uiTotalNumStreams, UMC_ERR_INVALID_PARAMS);
  UMC_CHECK(m_ppBuffers, UMC_ERR_NOT_INITIALIZED);

#ifdef VM_DEBUG
  Ipp64f pts, dts;
  lpData->GetTime(pts, dts);
  vm_debug_trace1(VM_DEBUG_PROGRESS, VM_STRING("Try to unlock buffer #%d with:"), iTrack);
  vm_debug_trace1(VM_DEBUG_PROGRESS, VM_STRING("    data_size = %d"), lpData->GetDataSize());
  vm_debug_trace2(VM_DEBUG_PROGRESS, VM_STRING("    data_time = (%.3f; %.3f)"), pts, dts);
#endif //VM_DEBUG

  UMC_CALL(m_ppBuffers[iTrack]->UnLockInputBuffer(lpData));
  return UMC_OK;
} //Status Muxer::UnlockBuffer(MediaData *lpData, Ipp32u iTrack)

Status Muxer::PutData(MediaData *lpData, Ipp32s iTrack)
{
  MediaData data;
  size_t dataSize = lpData->GetDataSize();
  void *pData = lpData->GetDataPointer();

  UMC_CALL(LockBuffer(&data, iTrack));

  // copy data
  UMC_CHECK(dataSize <= data.GetBufferSize(), UMC_ERR_NOT_ENOUGH_BUFFER);
  MFX_INTERNAL_CPY_S((Ipp8u*)data.GetDataPointer(), (Ipp32s)data.GetDataSize(), (Ipp8u*)pData, (Ipp32s)dataSize);

  // copy time & frame type
  Ipp64f dPTS, dDTS;
  lpData->GetTime(dPTS, dDTS);
  data.SetTime(dPTS, dDTS);
  data.SetDataSize(dataSize);
  data.SetFrameType(lpData->GetFrameType());

  UMC_CALL(UnlockBuffer(&data, iTrack));
  return UMC_OK;
} //Status Muxer::PutData(MediaData *lpData, Ipp32u iTrack)

Status Muxer::PutEndOfStream(Ipp32s iTrack)
{
  UMC_CHECK(m_ppBuffers, UMC_ERR_NOT_INITIALIZED);
  UMC_CHECK(iTrack >= 0, UMC_ERR_INVALID_PARAMS);
  UMC_CHECK((Ipp32u)iTrack < m_uiTotalNumStreams, UMC_ERR_INVALID_PARAMS);

  UMC_CALL(m_ppBuffers[iTrack]->UnLockInputBuffer(NULL, UMC_ERR_END_OF_STREAM));
  return UMC_OK;
} //Status Muxer::PutEndOfStream(Ipp32u iTrack)

Status Muxer::PutVideoData(MediaData *lpData, Ipp32s index)
{
  UMC_CALL(PutData(lpData, GetTrackIndex(VIDEO_TRACK, index)));
  return UMC_OK;
} //Status Muxer::PutVideoData(MediaData *lpData)

Status Muxer::PutAudioData(MediaData *lpData, Ipp32s index)
{
  UMC_CALL(PutData(lpData, GetTrackIndex(AUDIO_TRACK, index)));
  return UMC_OK;
} //Status Muxer::PutAudioData(MediaData *lpData)

Status Muxer::GetStreamToWrite(Ipp32s &rStreamNumber, bool bFlushMode)
{
  static const Ipp64f MAXIMUM_DOUBLE = 1.7E+308;

  Status umcRes;
  Ipp32u streamNum, minNum = 0;
  Ipp64f streamTime, minTime = MAXIMUM_DOUBLE;

  for (streamNum = 0; streamNum < (Ipp32u)m_uiTotalNumStreams; streamNum++)
  {
    umcRes = GetOutputTime(streamNum, streamTime);
    if (UMC_ERR_NOT_ENOUGH_DATA == umcRes && !bFlushMode)
    {
      vm_debug_trace1(VM_DEBUG_PROGRESS, VM_STRING("Ordering of elementary streams... stream #%d is empty"), streamNum);
      return umcRes;
    }

    if (UMC_OK == umcRes)
    {
      if (streamTime < minTime)
      {
        minNum = streamNum;
        minTime = streamTime;
      }
    }
  }

  // no more data in buffers
  if (minTime >= MAXIMUM_DOUBLE)
  {
    vm_debug_trace(VM_DEBUG_INFO, VM_STRING("END_OF_STREAM. All data was written"));
    return UMC_ERR_END_OF_STREAM;
  }

  rStreamNumber = minNum;
  vm_debug_trace2(VM_DEBUG_PROGRESS, VM_STRING("Ordering of elementary streams... stream #%d has min time (%.4f sec)"), minNum, minTime);
  return UMC_OK;
} //Status Muxer::GetStreamToWrite(Ipp32s &rStreamNumber, bool bFlushMode)
