//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

#include <string.h>
#include "umc_splitter.h"

namespace UMC
{

void SplitterInfo::DeprecatedSplitterInfo()
{
    // DEPRECATED !!!
    m_audio_info_aux = NULL;
    m_video_info_aux = NULL;
    number_audio_tracks = 0;
    number_video_tracks = 0;
    memset(&m_audio_info, 0, sizeof(m_audio_info));
    memset(&m_video_info, 0, sizeof(m_video_info));
    memset(&m_system_info, 0, sizeof(m_system_info));
} // SplitterInfo::SplitterInfo()

void Splitter::DeprecatedSplitter()
{
  pNewSplInfo = NULL;
  memset(pFirstFrame, 0, 32);
  //ippsZero_8u(pFirstFrame, 32); // TEMPORAL !!!
  memset(pAudioTrTbl, 0xFF, 32);
  //ippsSet_8u(0xFF, pAudioTrTbl, 32); // TEMPORAL !!!
  memset(pVideoTrTbl, 0xFF, 32);
  //ippsSet_8u(0xFF, pVideoTrTbl, 32); // TEMPORAL !!!
}

Status Splitter::GetNextAudioData(MediaData* data, Ipp32u num)
{
  Status umcRes;
  if (!pNewSplInfo)
  {
    SplitterInfo tmp;
    GetInfo(&tmp);
  }

  umcRes = GetBaseNextData(data, pAudioTrTbl[num], false);
  return umcRes;
}

Status Splitter::CheckNextAudioData(MediaData* data, Ipp32u num)
{
  Status umcRes;
  if (!pNewSplInfo)
  {
    SplitterInfo tmp;
    GetInfo(&tmp);
  }

  umcRes = GetBaseNextData(data, pAudioTrTbl[num], true);
  return umcRes;
}

Status Splitter::GetNextAudioData(MediaData* data)
{
    return GetNextAudioData(data, 0);
}

Status Splitter::GetNextVideoData(MediaData* data, Ipp32u num)
{
  Status umcRes;

  if (!pNewSplInfo)
  {
      SplitterInfo tmp;
      GetInfo(&tmp);
  }

  umcRes = GetBaseNextData(data, pVideoTrTbl[num], false);
  return umcRes;
}

Status Splitter::CheckNextVideoData(MediaData* data, Ipp32u num)
{
  Status umcRes;
  if (!pNewSplInfo)
  {
      SplitterInfo tmp;
      GetInfo(&tmp);
  }

  umcRes = GetBaseNextData(data, pVideoTrTbl[num], true);
  return umcRes;
}

Status Splitter::GetNextVideoData(MediaData* data)
{
  return GetNextVideoData(data, 0);
}

Status Splitter::GetBaseNextData(
    MediaData* data,
    Ipp32u nTrack,
    bool bCheck) // TEMPORAL !!!
{
  if (!pFirstFrame[nTrack] && pNewSplInfo) {
    *data = *pNewSplInfo->m_ppTrackInfo[nTrack]->m_pDecSpecInfo;
    data->SetTime(0, 0);
    pFirstFrame[nTrack] = 1;
    return UMC_OK;
  } else {
    return bCheck ? CheckNextData(data, nTrack) : GetNextData(data, nTrack);
  }
}

Status Splitter::GetInfo(SplitterInfo* pInfo) { // DEPRECATED !!!
  Status umcRes;
  Ipp32u i;

  umcRes = GetInfo((SplitterInfo **)&pNewSplInfo);
  UMC_CHECK_STATUS(umcRes)

  pInfo->m_system_info.stream_type = pNewSplInfo->m_SystemType;
  pInfo->m_splitter_flags = pNewSplInfo->m_splitter_flags;

  /* filling number of tracks */
  pInfo->number_audio_tracks = 0;
  pInfo->number_video_tracks = 0;
  for (i = 0; i < pNewSplInfo->m_nOfTracks; i++) {
    if (!pNewSplInfo->m_ppTrackInfo[i]->m_pDecSpecInfo) {
      pFirstFrame[i] = 1;
    }
    if (pNewSplInfo->m_ppTrackInfo[i]->m_isSelected) {
      if(pNewSplInfo->m_ppTrackInfo[i]->m_Type & TRACK_ANY_AUDIO) {
        pAudioTrTbl[pInfo->number_audio_tracks] = (Ipp8u)i;
        pInfo->number_audio_tracks++;
      } else if(pNewSplInfo->m_ppTrackInfo[i]->m_Type & TRACK_ANY_VIDEO) {
        pVideoTrTbl[pInfo->number_video_tracks] = (Ipp8u)i;
        pInfo->number_video_tracks++;
      }
    }
  }

  if (!pInfo->number_audio_tracks)
      pInfo->m_splitter_flags = pNewSplInfo->m_splitter_flags &= ~AUDIO_SPLITTER;
  if (!pInfo->number_video_tracks)
      pInfo->m_splitter_flags = pNewSplInfo->m_splitter_flags &= ~VIDEO_SPLITTER;

  if ((pInfo->number_audio_tracks > 0) && (pNewSplInfo->m_splitter_flags & AUDIO_SPLITTER)) {
    Ipp32u num = 0;
    Ipp32u uiPin = pAudioTrTbl[num];

    pInfo->m_audio_info = *((AudioStreamInfo*)pNewSplInfo->m_ppTrackInfo[uiPin]->m_pStreamInfo);

    //if any additional audio track
    if (pInfo->number_audio_tracks > 1) {
      if(pInfo->m_audio_info_aux) {
        free/*ippsFree*/(pInfo->m_audio_info_aux);
        pInfo->m_audio_info_aux = NULL;
      }

      Ipp32u buff_size = (pInfo->number_audio_tracks - 1) * sizeof(sAudioStreamInfo);
      pInfo->m_audio_info_aux = (sAudioStreamInfo*)malloc(buff_size); //ippsMalloc_8u(buff_size);

      if(pInfo->m_audio_info_aux) {
        memset(pInfo->m_audio_info_aux, 0, buff_size);
      } else {
        vm_debug_trace(VM_DEBUG_PROGRESS, VM_STRING("MP4 Splitter: Not Enough Mem"));
        return UMC_ERR_FAILED;
      }

      for (i = 0; i < (Ipp32u)pInfo->number_audio_tracks - 1; i++) {
        num++;
        uiPin = pAudioTrTbl[num];
        pInfo->m_audio_info_aux[i] = *((AudioStreamInfo *)pNewSplInfo->m_ppTrackInfo[uiPin]->m_pStreamInfo);
      }
    }
  }

  if ((pInfo->number_video_tracks > 0) && (pNewSplInfo->m_splitter_flags & VIDEO_SPLITTER)) {

    Ipp32u num = 0;
    Ipp32u uiPin = pVideoTrTbl[num];

    pInfo->m_video_info = *((VideoStreamInfo *)pNewSplInfo->m_ppTrackInfo[uiPin]->m_pStreamInfo);

    //if any additional video track, then information about them is also filled
    if (pInfo->number_video_tracks > 1) {
      //alloc memory
      if (pInfo->m_video_info_aux) {
          free/*ippsFree*/(pInfo->m_video_info_aux);
      }

      Ipp32u buff_size = (pInfo->number_video_tracks - 1) * sizeof(sVideoStreamInfo);
      pInfo->m_video_info_aux = (sVideoStreamInfo*)malloc/*ippsMalloc_8u*/(buff_size);
      if (pInfo->m_video_info_aux) {
          memset(pInfo->m_video_info_aux, 0, buff_size);
      } else {
        vm_debug_trace(VM_DEBUG_PROGRESS, VM_STRING("MP4 Splitter: Not Enough Mem"));
        return UMC_ERR_FAILED;
      }

      for (i = 0; i < (Ipp32u)pInfo->number_video_tracks - 1; i++) {
        num++;
        uiPin = pVideoTrTbl[num];
        pInfo->m_video_info_aux[i] = *((VideoStreamInfo *)pNewSplInfo->m_ppTrackInfo[uiPin]->m_pStreamInfo);
      }
    }
  }
  return UMC_OK;
}


} // namespace UMC
