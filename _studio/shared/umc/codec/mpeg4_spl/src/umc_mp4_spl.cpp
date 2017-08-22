//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include <ipps.h>
#include "ippdefs.h"
#include "vm_thread.h"
#include "vm_types.h"
#include "vm_time.h"
#include "umc_media_data.h"
#include "umc_cyclic_buffer.h"
#include "mp4cmn_config.h"
#include "bstream.h"
#include "umc_mp4_spl.h"
#include "umc_automatic_mutex.h"
#include "umc_structures.h"

namespace UMC
{

Splitter *CreateMPEG4Splitter() { return (new MP4Splitter()); }

MP4Splitter::MP4Splitter():
    IndexSplitter(),
    m_pInitMoofThread(NULL),
    m_nFragPosEnd(0),
    m_pFirstSegmentDuration(NULL),
    m_pLastPTS(NULL),
    m_bFlagStopInitMoof(false)
{
    memset(&m_headerMPEG4, 0, sizeof(m_headerMPEG4));
}

MP4Splitter::~MP4Splitter()
{
    Close();
}

Status MP4Splitter::CheckInit()
{
  // already initialized
  if ((m_pReader == NULL) ||
      (m_pTrackIndex == NULL) ||
      (m_ppMediaBuffer == NULL) ||
      (m_ppLockedFrame == NULL) ||
      (m_pIsLocked == NULL) ||
      (m_pInfo == NULL))
    return UMC_ERR_NOT_INITIALIZED;

  return UMC_OK;
}

Status MP4Splitter::CheckMoofInit()
{
  Ipp32u iES = 0;

  for (iES = 0; iES < m_headerMPEG4.moov.mvex.total_tracks; iES++) {
    if (!m_headerMPEG4.moov.trak[iES]->mdia.minf.stbl.stsz.max_sample_size) {
      return UMC_ERR_NOT_INITIALIZED;
    }
  }

  return UMC_OK;
}

Status MP4Splitter::Clear_track(T_trak_data* pTrak)
{
  Ipp32u j = 0;

  if (pTrak) {
    for (j = 0; j < pTrak->mdia.minf.stbl.stsd.total_entries; j++) {
      if (!pTrak->mdia.minf.stbl.stsd.table[j].esds.flags) {
        if (pTrak->mdia.minf.stbl.stsd.table[j].esds.decoderConfigLen) {
          ippsFree(pTrak->mdia.minf.stbl.stsd.table[j].esds.decoderConfig);
          pTrak->mdia.minf.stbl.stsd.table[j].esds.decoderConfig = NULL;
        }
      }
      if (pTrak->mdia.minf.stbl.stsd.table[j].avcC.decoderConfigLen) {
        ippsFree(pTrak->mdia.minf.stbl.stsd.table[j].avcC.decoderConfig);
        pTrak->mdia.minf.stbl.stsd.table[j].avcC.decoderConfig = NULL;
      }
      if (pTrak->mdia.minf.stbl.stsd.table[j].damr.decoderConfigLen) {
        ippsFree(pTrak->mdia.minf.stbl.stsd.table[j].damr.decoderConfig);
        pTrak->mdia.minf.stbl.stsd.table[j].damr.decoderConfig = NULL;
      }
    }

    if (!(pTrak->mdia.minf.stbl.stsz.flags)) {
      if (!pTrak->mdia.minf.stbl.stsz.sample_size) {
        if (pTrak->mdia.minf.stbl.stsz.table)
          ippsFree(pTrak->mdia.minf.stbl.stsz.table);
        pTrak->mdia.minf.stbl.stsz.table = NULL;
      }
    }

    if(!(pTrak->mdia.minf.stbl.stsc.flags)) {
      if (pTrak->mdia.minf.stbl.stsc.table)
        ippsFree(pTrak->mdia.minf.stbl.stsc.table);
      pTrak->mdia.minf.stbl.stsc.table = NULL;
    }

    if (pTrak->mdia.minf.stbl.ctts.table)
      ippsFree(pTrak->mdia.minf.stbl.ctts.table);
    pTrak->mdia.minf.stbl.ctts.table = NULL;


    if (pTrak->mdia.minf.stbl.co64.table)
      ippsFree(pTrak->mdia.minf.stbl.co64.table);
    pTrak->mdia.minf.stbl.co64.table = NULL;


    if(!(pTrak->mdia.minf.stbl.stco.flags)) {
      if (pTrak->mdia.minf.stbl.stco.table)
        ippsFree(pTrak->mdia.minf.stbl.stco.table);
      pTrak->mdia.minf.stbl.stco.table = NULL;
    }

    if(!(pTrak->mdia.minf.stbl.stts.flags)) {
      if (pTrak->mdia.minf.stbl.stts.table)
        ippsFree(pTrak->mdia.minf.stbl.stts.table);
      pTrak->mdia.minf.stbl.stts.table = NULL;
    }

    if(!(pTrak->mdia.minf.stbl.stss.flags)) {
      if (pTrak->mdia.minf.stbl.stss.table)
        ippsFree(pTrak->mdia.minf.stbl.stss.table);
      pTrak->mdia.minf.stbl.stss.table = NULL;
    }

    if(!(pTrak->mdia.minf.stbl.stsd.flags)) {
      if (pTrak->mdia.minf.stbl.stsd.table)
        ippsFree(pTrak->mdia.minf.stbl.stsd.table);
      pTrak->mdia.minf.stbl.stsd.table = NULL;
    }

    if (!(pTrak->mdia.minf.dinf.dref.flags)) {
      for (j = 0; j < pTrak->mdia.minf.dinf.dref.total_entries; j++) {
        if (pTrak->mdia.minf.dinf.dref.table[j].data_reference)
          ippsFree(pTrak->mdia.minf.dinf.dref.table[j].data_reference);
        pTrak->mdia.minf.dinf.dref.table[j].data_reference = NULL;
      }
      if (pTrak->mdia.minf.dinf.dref.table) {
        ippsFree(pTrak->mdia.minf.dinf.dref.table);
      }
      pTrak->mdia.minf.dinf.dref.table = NULL;
    }

    ippsFree(pTrak);
  }
  return UMC_OK;
}

void MP4Splitter::FillSampleSizeAndType(T_trak_data *trak,
                                        IndexFragment &frag)
{
  Ipp32u i;
  T_stsz_data stsz = trak->mdia.minf.stbl.stsz;
  T_stss_data stss = trak->mdia.minf.stbl.stss;

  for (i = 0; i < (Ipp32u)frag.iNOfEntries; i++) {
    if (stsz.sample_size == 0) {
      frag.pEntryArray[i].uiSize = trak->mdia.minf.stbl.stsz.table[i].size;
    } else {
      frag.pEntryArray[i].uiSize = stsz.sample_size;
    }
    if (stss.table == NULL) {
      frag.pEntryArray[i].uiFlags = I_PICTURE;
    }
  }
  if (stss.table) {
    for (i = 0; i < stss.total_entries; i++) {
      frag.pEntryArray[stss.table[i].sample_number - 1].uiFlags = I_PICTURE;
    }
  }
}

void MP4Splitter::FillSamplePos(T_trak_data *trak,
                                IndexFragment &frag)
{
  Ipp32u i;

  // chunk offset box (32 bit version)
  T_stco_data& stco = trak->mdia.minf.stbl.stco;
  // chunk offset box (64 bit version)
  T_co64_data& co64 = trak->mdia.minf.stbl.co64;
  // sample to chunk box
  T_stsc_data& stsc = trak->mdia.minf.stbl.stsc;

  Ipp64u nSampleOffset = 0;
  Ipp32u stco_i = 1;
  Ipp32u stsc_i = 0;
  Ipp32u samp_i = 0;

  while (samp_i < (Ipp32u)frag.iNOfEntries) {

    if (stco.total_entries) {
      if (stco_i > stco.total_entries)
        break;
      nSampleOffset = stco.table[stco_i - 1].offset;
    } else {
      if (stco_i > co64.total_entries)
        break;
      nSampleOffset = co64.table[stco_i - 1].offset;
    }

    /* skipping invalid entries */
    while (stsc_i < stsc.total_entries - 1) {
      if (stsc.table[stsc_i + 1].chunk - stsc.table[stsc_i].chunk <= 0) {
        stsc_i++;
      } else {
        break;
      }
    }

    for (i = 0; i < stsc.table[stsc_i].samples; i++) {
      frag.pEntryArray[samp_i].stPosition = (size_t)nSampleOffset;
      nSampleOffset += frag.pEntryArray[samp_i].uiSize;
      samp_i++;
      if (samp_i >= (Ipp32u)frag.iNOfEntries)
        break;
    }

    stco_i++;
    if (stsc_i < stsc.total_entries - 1 &&
        stco_i == stsc.table[stsc_i + 1].chunk) {
        stsc_i++;
    }

  }

}

void MP4Splitter::FillSampleTimeStamp(T_trak_data *trak,
                                      IndexFragment &frag)
{
  // decoding time to sample box
  T_stts_data& stts = trak->mdia.minf.stbl.stts;
  // composition time to sample box
  T_ctts_data& ctts = trak->mdia.minf.stbl.ctts;
  Ipp32s nTimeScale = trak->mdia.mdhd.time_scale;

  if (nTimeScale == 0) {
    frag.iNOfEntries = 0;
    return;
  }

  Ipp32u i;
  Ipp32u stts_i;
  Ipp32u samp_i = 0;
  Ipp32u ctts_i = 0;
  Ipp32u nTimeStamp = 0;

  for (stts_i = 0; stts_i < stts.total_entries; stts_i++) {
    for (i = 0; i < stts.table[stts_i].sample_count; i++) {
      if (ctts.total_entries == 0) { // composition time stamp not present
        frag.pEntryArray[samp_i].dPts = (Ipp64f)nTimeStamp / nTimeScale;
        frag.pEntryArray[samp_i].dDts = -1.0;
      } else {
        frag.pEntryArray[samp_i].dPts =
        frag.pEntryArray[samp_i].dDts = (Ipp64f)nTimeStamp;
      }
      nTimeStamp += stts.table[stts_i].sample_duration;
      samp_i++;
      if (samp_i >= (Ipp32u)frag.iNOfEntries)
        break;
    }
  }

  if (ctts.total_entries == 0) {
    if (samp_i < (Ipp32u)frag.iNOfEntries) {
      frag.iNOfEntries = samp_i;
    }
    return;
  }

  samp_i = 0;

  for (ctts_i = 0; ctts_i < ctts.total_entries; ctts_i++) {
    for (i = 0; i < ctts.table[ctts_i].sample_count; i++) {
      frag.pEntryArray[samp_i].dPts += ctts.table[ctts_i].sample_offset;
      frag.pEntryArray[samp_i].dPts /= nTimeScale;
      frag.pEntryArray[samp_i].dDts /= nTimeScale;
      samp_i++;
      if (samp_i >= (Ipp32u)frag.iNOfEntries)
        return;
    }
  }

  if (samp_i < (Ipp32u)frag.iNOfEntries)
    frag.iNOfEntries = samp_i;

}

Status MP4Splitter::AddMoovToIndex(Ipp32u iES)
{
  IndexFragment frag;
  T_trak_data *trak = m_headerMPEG4.moov.trak[iES];
  Ipp32u nEntries = trak->mdia.minf.stbl.stsz.total_entries;

  frag.iNOfEntries = nEntries;
  if (frag.iNOfEntries <= 0)
    return UMC_OK;
  frag.pEntryArray = new IndexEntry[nEntries];
  if (NULL == frag.pEntryArray)
      return UMC_ERR_ALLOC;

  FillSampleSizeAndType(trak, frag);
  FillSamplePos(trak, frag);
  FillSampleTimeStamp(trak, frag);

  m_pLastPTS[iES] = frag.pEntryArray[frag.iNOfEntries-1].GetTimeStamp();

  return m_pTrackIndex[iES].Add(frag);
}

Status MP4Splitter::AddMoofRunToIndex(Ipp32u iES, T_trex_data *pTrex, T_traf *pTraf,
                                      T_trun *pTrun, Ipp64u &nBaseOffset)
{
  IndexFragment frag;
  Ipp32u i;

  Ipp64f rTimeScale = m_headerMPEG4.moov.trak[iES]->mdia.mdhd.time_scale;
  if (rTimeScale <= 0)
    return UMC_ERR_FAILED;

  frag.iNOfEntries = pTrun->sample_count;
  frag.pEntryArray = new IndexEntry[pTrun->sample_count];
  if (NULL == frag.pEntryArray)
      return UMC_ERR_ALLOC;

  for (i = 0; i < pTrun->sample_count; i++) {
      if (m_bFlagStopInitMoof) {
        delete[] frag.pEntryArray;
        return UMC_OK;
      }

    // sample size present in trun
    if (pTrun->flags & SAMPLE_SIZE_PRESENT) {
      frag.pEntryArray[i].uiSize = pTrun->table_trun[i].sample_size;
    }
    // sample size present in track fragment header box (tfhd)
    else if (pTraf->tfhd.flags & DEFAULT_SAMPLE_SIZE_PRESENT) {
      frag.pEntryArray[i].uiSize = pTraf->tfhd.default_sample_size;
    } else {
      // default sample size in Track Extends box
      frag.pEntryArray[i].uiSize = pTrex->default_sample_size;
    }

    frag.pEntryArray[i].stPosition = (size_t)nBaseOffset;
    // update base offset for next sample
    nBaseOffset = frag.pEntryArray[i].stPosition + frag.pEntryArray[i].uiSize;

    Ipp32u duration = 0;
    // sample duration present in Track Fragment Run box
    if (pTrun->flags & SAMPLE_DURATION_PRESENT) {
      duration = pTrun->table_trun[i].sample_duration;
    }
    // sample duration present in Track Fragment Header box
    else if (pTraf->tfhd.flags & DEFAULT_SAMPLE_DURATION_PRESENT) {
      duration = pTraf->tfhd.default_sample_duration;
    } else {
      duration = pTrex->default_sample_duration;
    }

    if (m_headerMPEG4.moov.trak[iES]->mdia.minf.is_video) {
      VideoStreamInfo *pVInfo = (VideoStreamInfo*)m_pInfo->m_ppTrackInfo[iES]->m_pStreamInfo;
      if (pVInfo->framerate <= 0) { /*** framerate is not set ***/
        pVInfo->framerate = rTimeScale/duration;
      }
    }

      // sample-composition-time-offset-present
    if (pTrun->flags & SAMPLE_COMPOSITION_TIME_OFFSETS_PRESENT) {
        duration += pTrun->table_trun[i].sample_composition_time_offset;
    }

    if (rTimeScale) {
      frag.pEntryArray[i].dPts = duration/rTimeScale;
    }

    frag.pEntryArray[i].dPts += m_pLastPTS[iES];
    m_pLastPTS[iES] = frag.pEntryArray[i].GetTimeStamp();

  }

  return m_pTrackIndex[iES].Add(frag);
}

Ipp32u MP4Splitter::InitMoofThreadCallback(void* pParam)
{
  VM_ASSERT(NULL != pParam);
  UMC::MP4Splitter* pThis = (MP4Splitter*)pParam;
  pThis->InitMoof();
  return 0;
}

Status MP4Splitter::MapTrafIDToTrackID (Ipp32u trafID, Ipp32u &nTrack)
{
  Ipp32u i;

  for (i = 0; i < m_pInfo->m_nOfTracks; i++) {
    if (m_pInfo->m_ppTrackInfo[i]->m_PID == trafID) {
      nTrack = i;
      return UMC_OK;
    }
  }
  return UMC_ERR_FAILED;
}

/* reads the next fragment and adds it to the index */
Status MP4Splitter::InitMoof()
{
  Ipp32u iES, jES;
  Status umcRes = CheckInit();

  if (umcRes != UMC_OK)
    return umcRes;

  while (!m_bFlagStopInitMoof) {
    umcRes = SelectNextFragment();
    UMC_CHECK_STATUS(umcRes)

    Ipp64u nBaseOffset = 0;
    for (iES = 0; iES < m_headerMPEG4.moof.total_tracks; iES++) {

      // track fragment
      T_traf* pTraf = m_headerMPEG4.moof.traf[iES];
      if (pTraf == NULL)
        continue;

      // track extend box
      T_trex_data* pTrex = NULL;
      // finding a corresponding trex box based on track ID
      for (jES = 0; jES < m_headerMPEG4.moov.mvex.total_tracks; jES++) {
        if (m_headerMPEG4.moov.mvex.trex[jES] == NULL)
          continue;

        if (pTraf->tfhd.track_ID == m_headerMPEG4.moov.mvex.trex[jES]->track_ID) {
          pTrex = m_headerMPEG4.moov.mvex.trex[jES];
          break;
        }
      }
      if (pTrex == NULL)
          continue;

      if (pTraf->tfhd.flags & BASE_DATA_OFFSET_PRESENT) {
        // base data offset present
        nBaseOffset = pTraf->tfhd.base_data_offset;
      }

      if ((iES == 0) && (nBaseOffset == 0)) {
        // first byte after enclosing moof box;
        nBaseOffset = m_headerMPEG4.moof.end;

        // offset by 8 bytes (compact mdat box) or 16 bytes (large size)
        if (m_headerMPEG4.data.is_large_size) {
          nBaseOffset += 16;
        } else {
          nBaseOffset += 8;
        }
      }

      if (pTraf->tfhd.flags & DURATION_IS_EMPTY) {
          // duration is empty, no track run
          // TO DO: add one fragment containing only one sample, sample size and
          // position are both zero. sample time stamp is set to default sample duration.
      }

      Ipp64u nLastBaseOffset = nBaseOffset;
      for (Ipp32u k = 0; k < pTraf->entry_count; k++) {

        // zero or more Trak Run Boxes
        T_trun* pTrun = pTraf->trun[k];

        if ((pTrun == NULL) || (pTrun->sample_count == 0))
          continue;

        /*** where to connect current track fragment ***/
        Ipp32u nTrack = 0;
        umcRes = MapTrafIDToTrackID(pTraf->tfhd.track_ID, nTrack);
        if (umcRes != UMC_OK)
          continue;

        if (pTrun->flags & DATA_OFFSET_PRESENT) {
          // data-offset-present
          if (nBaseOffset + pTrun->data_offset > nLastBaseOffset)
            nLastBaseOffset = nBaseOffset + pTrun->data_offset;
        }

        umcRes = AddMoofRunToIndex(nTrack, pTrex, pTraf, pTrun, nLastBaseOffset);
        UMC_CHECK_STATUS(umcRes)

        /*** first data is read -> we can init buffer ***/
        if (!m_headerMPEG4.moov.trak[iES]->mdia.minf.stbl.stsz.max_sample_size) {
          MediaBufferParams mParams;
          Ipp32u nBufferSize = 0;
          IndexEntry entry;

          UMC_NEW(m_ppMediaBuffer[iES], SampleBuffer);
          UMC_NEW(m_ppLockedFrame[iES], MediaData);
          m_pIsLocked[iES] = 0;
          mParams.m_numberOfFrames = NUMBEROFFRAMES;

          if (m_headerMPEG4.moov.trak[iES]->mdia.minf.is_audio) {
              nBufferSize = MINAUDIOBUFFERSIZE;
          } else if (m_headerMPEG4.moov.trak[iES]->mdia.minf.is_video) {
              nBufferSize = MINVIDEOBUFFERSIZE;
          }

          mParams.m_prefInputBufferSize =  mParams.m_prefOutputBufferSize = nBufferSize;
          UMC_CALL(m_ppMediaBuffer[iES]->Init(&mParams));

          m_headerMPEG4.moov.trak[iES]->mdia.minf.stbl.stsz.max_sample_size = nBufferSize;

          umcRes = m_pTrackIndex[iES].First(entry);
          if (umcRes != UMC_OK) {
            vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("Failed to add fragment!!!\n"));
            return umcRes;
          }
        }

      }

      nBaseOffset = nLastBaseOffset;
    }

  }

  return UMC_OK;
}


/*
AudioStreamType AudioObjectTypeID(Ipp32u value)
{
  switch(value)
  {
    case 0x6b:
      return UMC::MPEG1_AUDIO;
    case 0x69:
      return UMC::MPEG2_AUDIO;
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x06:
      return UMC::AAC_MPEG4_STREAM;
    default:
      return UMC::UNDEF_AUDIO;
  }
}
*/

VideoStreamType GetVideoStreamType(Ipp8u value)
{
  switch (value) {
    case 0x20:
      return MPEG4_VIDEO;
    case 0x60:  // 13818-2
    case 0x61:  // 13818-2
    case 0x62:  // 13818-2
    case 0x63:  // 13818-2
    case 0x64:  // 13818-2
    case 0x65:  // 13818-2
      return MPEG2_VIDEO;
    case 0x6A:
      return MPEG1_VIDEO;
    case 0x6C:  // 10918-1
      return MJPEG_VIDEO;
    case 0xf1:
      return H264_VIDEO;
    case 0xf2:
      return H263_VIDEO;
    case 0xf3:
      return H261_VIDEO;
    case 0xf4:
      return AVS_VIDEO;
    case 0xf5:
      return HEVC_VIDEO;
    default:
      return UNDEF_VIDEO;
  }
}

AudioStreamType GetAudioStreamType(Ipp8u value)
{
  switch (value) {
  case 0x40:  // 14496-3
  case 0x66:  // 13818-7
  case 0x67:  // 13818-7
  case 0x68:  // 13818-7
    return AAC_MPEG4_STREAM;
  case 0x69:  // 13818-3
    return MPEG2_AUDIO;
  case 0x6B:  // 11172-3
    return MPEG1_AUDIO;
  default:
    return UNDEF_AUDIO;
  }
}

Ipp32u GetChannelMask(Ipp32s channels)
{
  switch(channels) {
    case 1:
      return  CHANNEL_FRONT_CENTER;
    case 2:
      return  CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT;
    case 3:
      return  CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER;
    case 4:
      return  CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER |
              CHANNEL_TOP_CENTER;
    case 5:
      return  CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER |
              CHANNEL_BACK_LEFT  | CHANNEL_BACK_RIGHT;
    case 6:
      return  CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER |
              CHANNEL_BACK_LEFT  | CHANNEL_BACK_RIGHT | CHANNEL_LOW_FREQUENCY;
    case 7:
      return  CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER |
              CHANNEL_BACK_LEFT  | CHANNEL_BACK_RIGHT | CHANNEL_LOW_FREQUENCY |
              CHANNEL_FRONT_LEFT_OF_CENTER | CHANNEL_FRONT_RIGHT_OF_CENTER;
    default:
      return 0;
  }
}

const AudioStreamType MPEGAStreamType[2][3] = {
    {MP2L1_AUDIO, MP2L2_AUDIO, MP2L3_AUDIO},
    {MP1L1_AUDIO, MP1L2_AUDIO, MP1L3_AUDIO}
};

// macro for mp3 header parsing (copied from frame_constructor)
#define MPEGA_HDR_VERSION(x)       ((x & 0x80000) >> 19)
#define MPEGA_HDR_LAYER(x)         (4 - ((x & 0x60000) >> 17))
#define MPEGA_HDR_ERRPROTECTION(x) ((x & 0x10000) >> 16)
#define MPEGA_HDR_BITRADEINDEX(x)  ((x & 0x0f000) >> 12)
#define MPEGA_HDR_SAMPLINGFREQ(x)  ((x & 0x00c00) >> 10)
#define MPEGA_HDR_PADDING(x)       ((x & 0x00200) >> 9)
#define MPEGA_HDR_MODE(x)          ((x & 0x000c0) >> 6)

Status MP4Splitter::FillMPEGAudioInfo(Ipp32u nTrack)
{
  Status umcRes;
  TrackIndex *pIndex = &m_pTrackIndex[nTrack];
  IndexEntry m_Frame;
  Ipp8u *p_frame;
  Ipp32u curPos = 0, header = 0;
  Ipp32s id = 0, layer = 0, bitrate = 0, freq = 0, mode = 0, padding = 0, protection = 0;
//  bool bSyncWordFound = false;
  AudioStreamInfo *pAudioInfo = (AudioStreamInfo *)(m_pInfo->m_ppTrackInfo[nTrack]->m_pStreamInfo);
  Ipp32s mpg25 = 0;

  // get first frame (index is already set to First frame)
  umcRes = pIndex->Get(m_Frame);
  UMC_CHECK_STATUS(umcRes)

  p_frame = new Ipp8u[m_Frame.uiSize];

  m_ReaderMutex.Lock();
  m_pReader->SetPosition((Ipp64u)m_Frame.stPosition);
  m_pReader->GetData(p_frame, &m_Frame.uiSize);
  m_ReaderMutex.Unlock();

  for (curPos = 0; curPos < m_Frame.uiSize - 3; curPos++)
  {
    // find synchroword
    if (p_frame[curPos] != 0xff || (p_frame[curPos + 1] & 0xe0) != 0xe0)
      continue;

    header = (p_frame[curPos + 0] << 24) |
             (p_frame[curPos + 1] << 16) |
             (p_frame[curPos + 2] <<  8) |
             (p_frame[curPos + 3]);

    id = MPEGA_HDR_VERSION(header);
    layer = MPEGA_HDR_LAYER(header);
    freq = MPEGA_HDR_SAMPLINGFREQ(header);
    bitrate = MPEGA_HDR_BITRADEINDEX(header);
    mode = MPEGA_HDR_MODE(header);
    padding = MPEGA_HDR_PADDING(header);
    protection = MPEGA_HDR_ERRPROTECTION(header);
    mpg25 = ((header >> 20) & 1) ? 0 : 2;

    // check forbidden values
    if ( 4 == layer   ||
         3 == freq    ||
#ifdef FREE_FORMAT_PROHIBIT
         0 == bitrate ||
#endif //FREE_FORMAT_PROHIBIT
        15 == bitrate ||
        (mpg25 > 0 && id == 1)) // for mpeg2.5 id should be 0
        continue;

//    bSyncWordFound = true;

/*
    Ipp32s size = 0;

    // evaluate frame size
    if (layer == 3)
        size = 72000 * (id + 1);
    else if (layer == 2)
        size = 72000 * 2;
    else if (layer == 1)
        size = 12000;

    size = size * MpegABitrate[id][layer - 1][bitrate] / MpegAFrequency[id + mpg25][freq] + padding;
    if (layer == 1)
        size *= 4;

    if (curPos + size >= m_lLastBytePos - 3)
        return UMC_ERR_NOT_ENOUGH_DATA;

    // check syncword of next frame
    if (buf[curPos + size] != 0xff || ((buf[curPos + size + 1] & 0xe0) != 0xe0)) // check syncword
    {
        bSyncWordFound = false;
        continue;
    }

    nextHeader = (m_pBuf[curPos + size + 0] << 16) |
                 (m_pBuf[curPos + size + 1] <<  8) |
                 (m_pBuf[curPos + size + 2]);

    // compare headers
    if ((nextHeader ^ (header >> 8)) & 0xfffe0c)
    {
        bSyncWordFound = false;
        continue;
    }

    // found header is valid
    bFound = true;*/
    break;
  }

/*
  m_lCurPos = curPos;
  if (!bFound)
    return bSyncWordFound ? UMC_ERR_NOT_ENOUGH_DATA : UMC_ERR_SYNC;
*/

  // fill info structure
/*
  pASI->is_protected     = protection == 0;
  pASI->sample_frequency = MpegAFrequency[id + mpg25][freq];
  pASI->channels         = MpegAChannels[mode];
  pASI->bitPerSample     = 16;
  pASI->bitrate          = MpegABitrate[id][layer - 1][bitrate] * 1000;
  pASI->stream_type      = MpegAStreamType[id][layer - 1];
  m_pInfo->m_Type        = ConvertAudioType(pASI->stream_type);
*/

  pAudioInfo->stream_type = MPEGAStreamType[id][layer - 1];

  delete[] p_frame;
  return UMC_OK;
}

Status MP4Splitter::FillAudioInfo(T_trak_data *pTrak, Ipp32u nTrack)
{
  AudioStreamInfo *pAudioInfo = (AudioStreamInfo *)(m_pInfo->m_ppTrackInfo[nTrack]->m_pStreamInfo);
  T_stsd_table_data *table = pTrak->mdia.minf.stbl.stsd.table;
  Ipp8u *ptr = NULL;
  Ipp32s len = 0;

  /*** AMR audio ***/
  if (table->damr.decoderConfigLen) {
    len = table->damr.decoderConfigLen;
    ptr = table->damr.decoderConfig;
  } else
  /*** AAC audio ***/
  if (table->esds.decoderConfigLen) {
    len = table->esds.decoderConfigLen;
    ptr = table->esds.decoderConfig;
  }

  if (len) {
    MediaData *pDecSpecInfo = new MediaData(len);
    UMC_CHECK_PTR(pDecSpecInfo)
    memcpy_s(pDecSpecInfo->GetDataPointer(), len, ptr, len);
    pDecSpecInfo->SetDataSize(len);
    pDecSpecInfo->SetTime(0, 0);
    m_pInfo->m_ppTrackInfo[nTrack]->m_pDecSpecInfo = pDecSpecInfo;
  }

  if (table->damr.decoderConfigLen == 0) {
    pAudioInfo->bitPerSample = 16;
    pAudioInfo->bitrate = table->esds.avgBitrate;
    pAudioInfo->channels = table->channels;
    pAudioInfo->sample_frequency = table->sample_rate;
    pAudioInfo->channel_mask = GetChannelMask(table->channels);
#if defined(_WIN32_WCE) /* conversion from unsigned __int64 to double not implemented */
    Ipp64s s64_dur = (Ipp64s)(pTrak->mdia.mdhd.duration / 2);
    pAudioInfo->duration = (Ipp64f)s64_dur / pTrak->mdia.mdhd.time_scale;
    pAudioInfo->duration *= 2;
#else
    pAudioInfo->duration = (Ipp64f)pTrak->mdia.mdhd.duration / pTrak->mdia.mdhd.time_scale;
#endif
    pAudioInfo->is_protected = (table->is_protected) ? true : false;
    pAudioInfo->stream_type = GetAudioStreamType(table->esds.objectTypeID);
    pAudioInfo->stream_subtype = UNDEF_AUDIO_SUBTYPE;
    switch (pAudioInfo->stream_type) {
    case AAC_MPEG4_STREAM:
      m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_AAC;

      /*** update audio_info from DecSpecInfo ***/
      if (m_pInfo->m_ppTrackInfo[nTrack]->m_pDecSpecInfo) {
        sAudio_specific_config  audio_config_data;
        sBitsreamBuffer         bs;

//        T_mdia_data m_Mdia = m_headerMPEG4.moov.trak[nTrack]->mdia;

        INIT_BITSTREAM(&bs, m_pInfo->m_ppTrackInfo[nTrack]->m_pDecSpecInfo->GetDataPointer())
        bs.nDataLen = m_pInfo->m_ppTrackInfo[nTrack]->m_pDecSpecInfo->GetDataSize();
        if (!dec_audio_specific_config(&audio_config_data,&bs)) {
          //dec_audio_specific_config returns 0, if everything is OK.
//          pAudioInfo->stream_type = AudioObjectTypeID(audio_config_data.audioObjectType);
          pAudioInfo->stream_type = UMC::AAC_MPEG4_STREAM;
          pAudioInfo->sample_frequency = get_sampling_frequency(&audio_config_data, 0);

          if (1 == audio_config_data.sbrPresentFlag) {
            pAudioInfo->sample_frequency = get_sampling_frequency(&audio_config_data, 1);
          }

          pAudioInfo->channels = get_channels_number(&audio_config_data);
        }
      }
      break;
    case MPEG2_AUDIO:
      m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_MPEGA;
      break;
    case MPEG1_AUDIO:
      m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_MPEGA;
      break;
    default:
      m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_UNKNOWN;
    }
  } else {  /*** AMR audio ***/
    pAudioInfo->sample_frequency = 8000;
    pAudioInfo->channels = 1;
    pAudioInfo->bitPerSample = 16;
    pAudioInfo->stream_type = AMR_NB_AUDIO;
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_AMR;
  }
  pAudioInfo->streamPID = pTrak->tkhd.track_id;

  return UMC_OK;
}

Status MP4Splitter::FillVideoInfo(T_trak_data *pTrak, Ipp32u nTrack)
{
  VideoStreamInfo *pVideoInfo = (VideoStreamInfo *)(m_pInfo->m_ppTrackInfo[nTrack]->m_pStreamInfo);
  T_stsd_table_data *table = pTrak->mdia.minf.stbl.stsd.table;
  Ipp8u *ptr = NULL;
  Ipp32s len = 0;
  Status umcRes = UMC_OK;

  pVideoInfo->clip_info.width = table->width;
  pVideoInfo->clip_info.height = table->height;
  pVideoInfo->aspect_ratio_width = 1;
  pVideoInfo->aspect_ratio_height = 1;
  pVideoInfo->color_format = YV12;

  /*** update clip_info and interlace_type ***/
  if (table->esds.decoderConfigLen) {
    len = table->esds.decoderConfigLen;
    ptr = table->esds.decoderConfig;
    //get bitrate
    pVideoInfo->bitrate = table->esds.avgBitrate;
    //get info about stream from ESDS info
    umcRes = ParseESDSHeader(pTrak, nTrack);
  } else if (table->avcC.decoderConfigLen) {
    len = table->avcC.decoderConfigLen;
    ptr = table->avcC.decoderConfig;
    //get info about stream from avcC info
    if (HEVC_VIDEO == table->avcC.type)
      umcRes = ParseHVCCHeader(pTrak, nTrack);
    else
      umcRes = ParseAVCCHeader(pTrak, nTrack);
  }
  UMC_CHECK_STATUS(umcRes)

  if (len) {
    MediaData *pDecSpecInfo = new MediaData(len);
    UMC_CHECK_PTR(pDecSpecInfo)
    memcpy_s(pDecSpecInfo->GetDataPointer(), len, ptr, len);
    pDecSpecInfo->SetDataSize(len);
    pDecSpecInfo->SetTime(0, 0);
    m_pInfo->m_ppTrackInfo[nTrack]->m_pDecSpecInfo = pDecSpecInfo;
  }
  pVideoInfo->streamPID = pTrak->tkhd.track_id;

  //get duration
#if defined(_WIN32_WCE) /* conversion from unsigned __int64 to double not implemented */
    Ipp64s s64_dur = (Ipp64s)(pTrak->mdia.mdhd.duration / 2);
    pVideoInfo->duration = (Ipp64f)s64_dur / pTrak->mdia.mdhd.time_scale;
    pVideoInfo->duration *= 2;
#else
    pVideoInfo->duration = (Ipp64f)pTrak->mdia.mdhd.duration / pTrak->mdia.mdhd.time_scale;
#endif

  //get framerate
  if (pVideoInfo->duration)
    pVideoInfo->framerate = (Ipp64f)pTrak->mdia.minf.stbl.stsz.total_entries / pVideoInfo->duration;

/*
  if (pVideoInfo->duration > 0 && !m_headerMPEG4.moov.mvex.total_tracks) {
      pInfo->m_video_info.framerate = (m_Mdia.minf.stbl.stsz.total_entries) /
                                        pInfo->m_video_info.duration;
  } else {
    if (m_headerMPEG4.moov.mvex.total_tracks && m_headerMPEG4.moov.mvex.trex[uiPin]->default_sample_duration) {
        pInfo->m_video_info.framerate = m_Mdia.mdhd.time_scale/
          m_headerMPEG4.moov.mvex.trex[uiPin]->default_sample_duration;
    } else {
          if (m_headerMPEG4.moov.mvex.trex[uiPin] &&
              m_headerMPEG4.moov.mvex.trex[uiPin]->default_sample_duration) {
            pInfo->m_video_info.framerate = m_headerMPEG4.moov.mvex.trex[uiPin]->default_sample_duration;
          }
          //if duration was changed, after parsing of the all stream fragments
          else if (m_pFirstSegmentDuration && m_pFirstSegmentDuration[uiPin] > 0) {
              pInfo->m_video_info.framerate = (Ipp64f)(m_Mdia.minf.stbl.stsz.total_entries) /
                                                  m_pFirstSegmentDuration[uiPin];
            pInfo->m_video_info.framerate *= m_Mdia.mdhd.time_scale;
          } else {
            pInfo->m_video_info.framerate = 0;
          }
        }
    }
*/

    //get stream type
  pVideoInfo->stream_type = GetVideoStreamType(table->esds.objectTypeID);
  pVideoInfo->stream_subtype = UNDEF_VIDEO_SUBTYPE;
  if (pVideoInfo->stream_type == MPEG4_VIDEO) {
    pVideoInfo->stream_subtype = MPEG4_VIDEO_QTIME;
  } else if (pVideoInfo->stream_type == H264_VIDEO) {
    pVideoInfo->stream_subtype = AVC1_VIDEO;
  }

  switch (pVideoInfo->stream_type) {
  case MPEG4_VIDEO:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_MPEG4V;
    break;
  case MPEG2_VIDEO:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_MPEG2V;
    break;
  case MPEG1_VIDEO:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_MPEG1V;
    break;
  case MJPEG_VIDEO:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_MJPEG;
    break;
  case H264_VIDEO:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_H264;
    break;
  case H263_VIDEO:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_H263;
    break;
  case H261_VIDEO:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_H261;
    break;
  case AVS_VIDEO:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_AVS;
    break;
  case HEVC_VIDEO:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_H265;
    break;
  default:
    m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_UNKNOWN;
  }

  return UMC_OK;
}

Status MP4Splitter::Init(SplitterParams& init)
{
  Ipp32u iES, nAudio = 0, nVideo = 0;
  IndexEntry entry;
  Status umcRes, umcResOut = UMC_OK;

  if (init.m_pDataReader == NULL)
    return UMC_ERR_NOT_ENOUGH_DATA;

  m_bFlagStop = true;

  /* init LockableDataReader */
  umcRes = IndexSplitter::Init(init);
  UMC_CHECK_STATUS(umcRes)

  m_pInfo = new SplitterInfo;
  UMC_CHECK_PTR(m_pInfo)

  m_pInfo->m_splitter_flags = init.m_lFlags;
  m_pInfo->m_SystemType = MP4_ATOM_STREAM;

  memset(&m_headerMPEG4, 0, sizeof(info_atoms));

  /* read moov atom */
  umcRes = ParseMP4Header();
  if (umcRes == UMC_WRN_INVALID_STREAM) {
    vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("ParseMP4Header() = UMC_WRN_INVALID_STREAM\n"));
    umcResOut = umcRes;
    umcRes = UMC_OK;
  }
  UMC_CHECK_STATUS(umcRes)

  m_pInfo->m_nOfTracks = m_headerMPEG4.moov.total_tracks;
#if defined(_WIN32_WCE) /* conversion from unsigned __int64 to double not implemented */
    Ipp64s s64_dur = (Ipp64s)(m_headerMPEG4.moov.mvhd.duration / 2);
    m_pInfo->m_dDuration = (Ipp64f)s64_dur / m_headerMPEG4.moov.mvhd.time_scale;
    m_pInfo->m_dDuration *= 2;
#else
    m_pInfo->m_dDuration = (Ipp64f)m_headerMPEG4.moov.mvhd.duration / m_headerMPEG4.moov.mvhd.time_scale;
#endif

  m_pTrackIndex = new TrackIndex[m_pInfo->m_nOfTracks];
  if (NULL == m_pTrackIndex)
      return UMC_ERR_ALLOC;

  m_pInfo->m_ppTrackInfo = new TrackInfo*[m_pInfo->m_nOfTracks];
  memset(m_pInfo->m_ppTrackInfo, 0, m_pInfo->m_nOfTracks*sizeof(TrackInfo*));
  m_ppMediaBuffer = new MediaBuffer*[m_pInfo->m_nOfTracks];
  memset(m_ppMediaBuffer, 0, m_pInfo->m_nOfTracks*sizeof(MediaBuffer*));
  m_ppLockedFrame = new MediaData*[m_pInfo->m_nOfTracks];
  memset(m_ppLockedFrame, 0, m_pInfo->m_nOfTracks*sizeof(MediaData*));
  m_pIsLocked = new Ipp32s[m_pInfo->m_nOfTracks];
  memset(m_pIsLocked, 0, m_pInfo->m_nOfTracks*sizeof(Ipp32s));

  UMC_NEW_ARR(m_pReadESThread, vm_thread, m_pInfo->m_nOfTracks)
  m_pLastPTS = new Ipp64f[m_pInfo->m_nOfTracks];
  memset(m_pLastPTS, 0, m_pInfo->m_nOfTracks*sizeof(Ipp64f));

  for (iES = 0; iES < m_pInfo->m_nOfTracks; iES++) {
    T_trak_data *pTrak = m_headerMPEG4.moov.trak[iES];
    MediaBufferParams mParams;
    int nBufferSize = 0;

    umcRes = AddMoovToIndex(iES);
    UMC_CHECK_STATUS(umcRes)

    m_pInfo->m_ppTrackInfo[iES] = new MP4TrackInfo;
    m_pInfo->m_ppTrackInfo[iES]->m_PID = pTrak->tkhd.track_id;
    MP4TrackInfo *pMP4TrackInfo = (MP4TrackInfo *)m_pInfo->m_ppTrackInfo[iES];
    pMP4TrackInfo->m_DpndPID = pTrak->tref.dpnd.idTrak;
    vm_thread_set_invalid(&m_pReadESThread[iES]);

    umcRes = m_pTrackIndex[iES].First(entry);
    if ((umcRes == UMC_ERR_NOT_ENOUGH_DATA) && (!m_headerMPEG4.moov.mvex.total_tracks)) {
      // data is absent really or it's a fragmented file
      m_pInfo->m_ppTrackInfo[iES]->m_isSelected = 0;
      umcRes = UMC_OK;  // valid situation when track is unknown
      continue;
    }

    if (pTrak->mdia.minf.is_audio) {
      nAudio++;
      m_pInfo->m_ppTrackInfo[iES]->m_pStreamInfo = new AudioStreamInfo;
      umcRes = FillAudioInfo(pTrak, iES);
      UMC_CHECK_STATUS(umcRes)
      if (m_pInfo->m_ppTrackInfo[iES]->m_Type == TRACK_MPEGA) {
        umcRes = FillMPEGAudioInfo(iES);
        UMC_CHECK_STATUS(umcRes)
      }
      if ((m_pInfo->m_dRate == 1) && (m_pInfo->m_splitter_flags & AUDIO_SPLITTER)) {
        m_pInfo->m_ppTrackInfo[iES]->m_isSelected = 1;
      }
    } else if (pTrak->mdia.minf.is_video) {
      nVideo++;
      m_pInfo->m_ppTrackInfo[iES]->m_pStreamInfo = new VideoStreamInfo;
      umcRes = FillVideoInfo(pTrak, iES);
      UMC_CHECK_STATUS(umcRes)
      if (m_pInfo->m_splitter_flags & VIDEO_SPLITTER) {
        m_pInfo->m_ppTrackInfo[iES]->m_isSelected = 1;
      }
    } else {
      continue; // skip unsupported stream
    }
/*

    umcRes = m_pTrackIndex[iES].First(entry);
    if ((umcRes == UMC_ERR_NOT_ENOUGH_DATA) && (!m_headerMPEG4.moov.mvex.total_tracks)) {
      // data is absent really or it's a fragmented file
      m_pInfo->m_ppTrackInfo[iES]->m_isSelected = 0;
      continue;
    }
*/

    /*** data may not be read for fragmented files ***/
    if (pTrak->mdia.minf.stbl.stsz.max_sample_size) {
      UMC_NEW(m_ppMediaBuffer[iES], SampleBuffer);
      UMC_NEW(m_ppLockedFrame[iES], MediaData);
      m_pIsLocked[iES] = 0;
      mParams.m_numberOfFrames = NUMBEROFFRAMES;
      nBufferSize = pTrak->mdia.minf.stbl.stsz.max_sample_size;

      if (pTrak->mdia.minf.is_audio) {
        if (nBufferSize < MINAUDIOBUFFERSIZE)
          nBufferSize = MINAUDIOBUFFERSIZE;
      } else if (pTrak->mdia.minf.is_video) {
        if (nBufferSize < MINVIDEOBUFFERSIZE)
          nBufferSize = MINVIDEOBUFFERSIZE;
      }

      mParams.m_prefInputBufferSize =  mParams.m_prefOutputBufferSize = nBufferSize;
      UMC_CALL(m_ppMediaBuffer[iES]->Init(&mParams));
    }
  }

  // update splitter flag
  if (nVideo == 0) {
    m_pInfo->m_splitter_flags &= ~VIDEO_SPLITTER;
  }
  if (nAudio == 0) {
    m_pInfo->m_splitter_flags &= ~AUDIO_SPLITTER;
  }

  if (m_headerMPEG4.moov.mvex.total_tracks) { // there are fragments
    Ipp32s res = 0;
    m_pInitMoofThread = new vm_thread;
    vm_thread_set_invalid(m_pInitMoofThread);
    res = vm_thread_create(m_pInitMoofThread, (vm_thread_callback)InitMoofThreadCallback, (void *)this);
    if (res != 1) {
      return UMC_ERR_FAILED;
    }
    do {
      vm_time_sleep(10);
      umcRes = CheckMoofInit();
    } while (umcRes != UMC_OK);
  }

  return (umcResOut != UMC_OK) ? umcResOut : umcRes;
}

Status MP4Splitter::Close()
{
  Ipp32u iES;
  Status umcRes = CheckInit();

  if (umcRes != UMC_OK)
    return umcRes;

  m_bFlagStopInitMoof = true;
  if (vm_thread_is_valid(m_pInitMoofThread)) {
    vm_thread_wait(m_pInitMoofThread);
    vm_thread_close(m_pInitMoofThread);
    vm_thread_set_invalid(m_pInitMoofThread);
  }

  IndexSplitter::Close();

  for (iES = 0; m_pInfo && (iES < m_pInfo->m_nOfTracks); iES++) {
    Clear_track(m_headerMPEG4.moov.trak[iES]);

//    UMC_CALL(m_ppMediaBuffer[iES]->Close());
    UMC_DELETE(m_ppMediaBuffer[iES])
    UMC_DELETE(m_ppLockedFrame[iES])
    if (m_pInfo->m_ppTrackInfo[iES]->m_pDecSpecInfo) {
        delete m_pInfo->m_ppTrackInfo[iES]->m_pDecSpecInfo;
    }
    if (m_pInfo->m_ppTrackInfo[iES]->m_pStreamInfo) {
        delete m_pInfo->m_ppTrackInfo[iES]->m_pStreamInfo;
    }
    if (m_pInfo->m_ppTrackInfo[iES]) {
        delete m_pInfo->m_ppTrackInfo[iES];
    }
  }

  /* if there are extended tracks */
  umcRes = Clear_moof(m_headerMPEG4.moof);
  UMC_CHECK_STATUS(umcRes)

  for (iES = 0; iES < m_headerMPEG4.moov.mvex.total_tracks; iES++) {
    delete[] m_headerMPEG4.moov.mvex.trex[iES];
  }

  delete[] m_pLastPTS;
  UMC_DELETE_ARR(m_pReadESThread)

  delete[] m_pIsLocked;
  delete[] m_ppLockedFrame;
  delete[] m_ppMediaBuffer;
  if (m_pInfo) {
      delete[] m_pInfo->m_ppTrackInfo;
  }

  if (m_pTrackIndex) {
    delete[] m_pTrackIndex;
    m_pTrackIndex = NULL;
  }

  if (m_pInfo) {
    delete m_pInfo;
    m_pInfo = NULL;
  }

  return UMC_OK;
}

/*
extern Ipp32s GetIntraSizeValue(Ipp8u * buf, Ipp8u nSize)
{
    switch(nSize)
    {
        case 2: return ((*buf) << 8)  | (*(buf + 1));
        case 3: return ((*buf) << 16) | (*(buf + 1) << 8) |  (*(buf + 2));
        case 4: return ((*buf) << 24) | (*(buf + 1) << 16) | (*(buf + 2) << 8) | (*(buf + 3));
    }
    return 0;
}
*/

/*
void SplitterMP4::GetPictureType(Ipp32s nTrack, SplSample* pSample)
{
  // unreferenced parameter, to disable compiler warning C4100
  UNREFERENCED_PARAMETER(nTrack);

  if (!pSample || !m_pDataReader)
      return;

  unsigned char   buf[70];
  Ipp32u          nSizeToRead = 64 + 5; //64 is maximum size for header

  vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("call m_dataReaderMutex.Lock\n"));
  m_dataReaderMutex.Lock();
  vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("end call m_dataReaderMutex.Lock\n"));

  m_pDataReader->SetPosition(pSample->m_nPosition);
  m_pDataReader->GetData(buf, &nSizeToRead);

  if (MPEG4_VIDEO == m_eVideoStreamType) {
    Ipp32u          nStartCodePosition = 0;
    bool            bStartCode = false;

    for (unsigned int i = 0; i < nSizeToRead; i++) {
      if (0xb6 == (unsigned int)buf[i]) {
        nStartCodePosition = i;
        bStartCode = true;
        break;
      }
    }

    if (bStartCode) {
      int nCodingType = 0;
      int nBytePos = nStartCodePosition + 1; // start code

      BitStreamReader bsReader;
      bsReader.Init(buf + nBytePos);
      nCodingType = bsReader.GetBits(2);

      switch (nCodingType) {
      case 0:
        pSample->m_frameType = I_PICTURE;
        break;
      case 1:
        pSample->m_frameType = P_PICTURE;
        break;
      case 2:
        pSample->m_frameType = B_PICTURE;
        break;
      case 3:
        pSample->m_frameType = D_PICTURE; // SPRITE picture?
        break;
      }
    }

  } else if (H264_VIDEO == m_eVideoStreamType) {
    Ipp32s iCode = 0;
    int nIntraSize = 0;
    int nFrameSize = 0;

    bool bSkip = true;
    while(bSkip) {
      iCode = buf[m_nH264FrameIntraSize];

      if (NAL_UT_IDR_SLICE == (iCode & NAL_UNITTYPE_BITS)) {
        pSample->m_frameType = I_PICTURE;
        break;
      } else if (NAL_UT_SLICE == (iCode & NAL_UNITTYPE_BITS)) {
        break;
      } else {
        nIntraSize = GetIntraSizeValue(buf, m_nH264FrameIntraSize);
        nFrameSize += nIntraSize;

        if (nFrameSize >= pSample->m_nSize)
            break;

        m_pDataReader->MovePosition(nIntraSize -  nSizeToRead + m_nH264FrameIntraSize);
        m_pDataReader->GetData(buf, &nSizeToRead);
      }
    }
  }
  vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("call m_dataReaderMutex.UnLock\n"));
  m_dataReaderMutex.Unlock();
  vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("end call m_dataReaderMutex.UnLock\n"));
}
*/

/*
Status MP4Splitter::GetInfo(SplitterInfo* pInfo) { // TEMPORAL !!!
  pInfo->m_system_info.stream_type = m_pInfo->systemType;
  pInfo->m_splitter_flags = m_pInfo->m_splitter_flags;

  / * filling number of tracks * /
  for (Ipp32u i = 0; i < m_pInfo->nOfTracks; i++) {
    if(m_pInfo->ppTrackInfo[i]->type & TRACK_ANY_AUDIO) {
      pInfo->number_audio_tracks++;
    } else if(m_pInfo->ppTrackInfo[i]->type & TRACK_ANY_VIDEO) {
      pInfo->number_video_tracks++;
    }
  }

  if ((pInfo->number_audio_tracks > 0) && (m_pInfo->m_splitter_flags & AUDIO_SPLITTER)) {
    sAudio_specific_config  audio_config_data;
    sBitsreamBuffer         bs;
    int i;

    Ipp32u num = 0;
//    Ipp32u uiPin = GetTrackNumber(TRACK_ANY_AUDIO, num, m_pInfo); // search for the first audio track
    Ipp32u uiPin = pAudioTrTbl[num];

    Ipp32s m_DecInfolen = m_pInfo->ppTrackInfo[uiPin]->nDecSpecInfoLen;
    T_mdia_data m_Mdia = m_headerMPEG4.moov.trak[uiPin]->mdia;

    //get info from esds atom
    if (m_DecInfolen) {
      bs.nBit_offset = 32;
      bs.pCurrent_dword = bs.pBuffer = (Ipp32u*)m_pInfo->ppTrackInfo[uiPin]->pDecSpecInfo;
      if (!dec_audio_specific_config(&audio_config_data,&bs)) {
        //dec_audio_specific_config returns 0, if everything is OK.
        pInfo->m_audio_info.stream_type = AudioObjectTypeID(audio_config_data.audioObjectType);
        pInfo->m_audio_info.sample_frequency = get_sampling_frequency(&audio_config_data, 0);

        if (1 == audio_config_data.sbrPresentFlag) {
          pInfo->m_audio_info.sample_frequency = get_sampling_frequency(&audio_config_data, 1);
        }

        pInfo->m_audio_info.channels = get_channels_number(&audio_config_data);
      }
    }
    //or get info from   damr atom for AMR audio
    else if (m_Mdia.minf.stbl.stsd.table->damr.decoderConfigLen) {
      pInfo->m_audio_info.sample_frequency = 8000;
      pInfo->m_audio_info.channels = 1;
      pInfo->m_audio_info.stream_type = AMR_NB_AUDIO;
    }

    //get info about stream
    if (UNDEF_AUDIO ==  pInfo->m_audio_info.stream_type) {
      //get channels num
      pInfo->m_audio_info.channels = m_Mdia.minf.stbl.stsd.table->channels;

      //get sample frequency
      pInfo->m_audio_info.sample_frequency = m_Mdia.minf.stbl.stsd.table->sample_rate;

      //get audio stream type
      pInfo->m_audio_info.stream_type = GetAudioStreamType(m_Mdia.minf.stbl.stsd.table->esds.objectTypeID);

      //if audio stream type is AAC, then if the content protected is checked
      if (AAC_MPEG4_STREAM == pInfo->m_audio_info.stream_type &&
        m_Mdia.minf.stbl.stsd.table->is_protected) {
          pInfo->m_audio_info.is_protected = 1;
      }
    }

    //get channels mask
    pInfo->m_audio_info.channel_mask = GetChannelMask(pInfo->m_audio_info.channels);
    //get bitrate
    pInfo->m_audio_info.bitrate = m_Mdia.minf.stbl.stsd.table->esds.avgBitrate;
    pInfo->m_audio_info.bitPerSample = 0;
    //get duration
    pInfo->m_audio_info.duration = ((float)(Ipp64s)(m_Mdia.mdhd.duration) /
                                    (float)m_Mdia.mdhd.time_scale);

    //if any additional audio track
    if (pInfo->number_audio_tracks > 1) {
      if(pInfo->m_audio_info_aux) {
        ippsFree(pInfo->m_audio_info_aux);
      }
/ *
      unsigned int buff_size = (m_nTotalAudioTrack - 1) * sizeof(sAudioStreamInfo);
      pInfo->m_audio_info_aux = (sAudioStreamInfo*)ippsMalloc_8u(buff_size);

      if(pInfo->m_audio_info_aux) {
        memset(pInfo->m_audio_info_aux, 0, buff_size);
      } else {
        vm_debug_trace(VM_DEBUG_PROGRESS, VM_STRING("MP4 Splitter: Not Enough Mem"));
        return UMC_ERR_FAILED;
      }

      for (i = 0; i < m_nTotalAudioTrack - 1; i++) {
        num++;
        uiPin = GetTrackNumber(AUDIO_TRACK, num);

        m_Mdia = m_headerMPEG4.moov.trak[uiPin]->mdia;

        if (m_Mdia.minf.stbl.stsd.table->esds.decoderConfigLen) {
          bs.nBit_offset = 32;
          bs.pCurrent_dword =
              bs.pBuffer = (Ipp32u*) m_Mdia.minf.stbl.stsd.table->esds.decoderConfig;

          dec_audio_specific_config(&audio_config_data,&bs);

          pInfo->m_audio_info_aux[i].stream_type = ObjectIDType(audio_config_data.audioObjectType);
          pInfo->m_audio_info_aux[i].sample_frequency = get_sampling_frequency(&audio_config_data, 0);

          if (1 == audio_config_data.sbrPresentFlag) {
            pInfo->m_audio_info_aux[i].sample_frequency = get_sampling_frequency(&audio_config_data, 1);
          }

          pInfo->m_audio_info_aux[i].channels = get_channels_number(&audio_config_data);
        } else if (m_Mdia.minf.stbl.stsd.table->damr.decoderConfigLen) {
          pInfo->m_audio_info_aux[i].sample_frequency = 8000;
          pInfo->m_audio_info_aux[i].channels = 1;
          pInfo->m_audio_info_aux[i].stream_type = AMR_NB_AUDIO;
        }

        if (UMC::UNDEF_AUDIO == pInfo->m_audio_info_aux[i].stream_type) {
          pInfo->m_audio_info_aux[i].channels = m_Mdia.minf.stbl.stsd.table->channels;
          pInfo->m_audio_info_aux[i].sample_frequency = m_Mdia.minf.stbl.stsd.table->sample_rate;

          //get audio stream type
          GetAudioStreamType(m_Mdia.minf.stbl.stsd.table->esds.objectTypeID, &pInfo->m_audio_info_aux[i]);
        }

        if (AAC_MPEG4_STREAM == pInfo->m_audio_info_aux[i].stream_type &&
          m_Mdia.minf.stbl.stsd.table->is_protected) {
          pInfo->m_audio_info_aux[i].is_protected = 1;
        }

        pInfo->m_audio_info_aux[i].channel_mask = GetChannelsMask(pInfo->m_audio_info.channels);
        pInfo->m_audio_info_aux[i].bitrate = m_Mdia.minf.stbl.stsd.table->esds.avgBitrate;
        pInfo->m_audio_info_aux[i].bitPerSample = 0;
        pInfo->m_audio_info_aux[i].duration = ((Ipp32f)(Ipp64s)(m_Mdia.mdhd.duration) /
                                               (Ipp32f)m_Mdia.mdhd.time_scale);

        pInfo->m_audio_info_aux[i].bitPerSample  = 16;
      } // for ( total_audio )* /
    } // if ( total_audio > 1 )
    pInfo->m_audio_info.bitPerSample  = 16;
  } // if ( total_audio )

  if ((pInfo->number_video_tracks > 0) && (m_pInfo->m_splitter_flags & VIDEO_SPLITTER)) {

    Status umcRes;
    Ipp32u num = 0;
//    Ipp32u uiPin = GetTrackNumber(TRACK_ANY_VIDEO, num, m_pInfo); // search for the first video track
    Ipp32u uiPin = pVideoTrTbl[num];

//    Ipp32s m_DecInfolen = m_pInfo->ppTrackInfo[uiPin]->nDecSpecInfoLen;

    T_mdia_data m_Mdia = m_headerMPEG4.moov.trak[uiPin]->mdia;
    int i;

    //default values (not from ESDS info)
    pInfo->m_video_info.clip_info.width  = (int)m_Mdia.minf.stbl.stsd.table->width;
    pInfo->m_video_info.clip_info.height = (int)m_Mdia.minf.stbl.stsd.table->height;
    pInfo->m_video_info.aspect_ratio_height = 1;
    pInfo->m_video_info.aspect_ratio_width = 1;

    if (m_pInfo->ppTrackInfo[uiPin]->nDecSpecInfoLen) {
      pInfo->m_video_info = *(DynamicCast<VideoStreamInfo>(m_pInfo->ppTrackInfo[uiPin]->pStreamInfo));
    }

/ *
    if (m_Mdia.minf.stbl.stsd.table->esds.decoderConfigLen) {
      //get info about stream from ESDS info
      umcRes = ParseESDSHeader(m_Mdia.minf.stbl.stsd.table->esds, &pInfo->m_video_info, uiPin);
      UMC_CHECK_STATUS(umcRes)
    } else if (m_Mdia.minf.stbl.stsd.table->avcC.decoderConfigLen) {
      //get info about stream from avcC info
      umcRes = ParseAVCCHeader(m_Mdia.minf.stbl.stsd.table->avcC, &pInfo->m_video_info);
      UMC_CHECK_STATUS(umcRes)
    }
* /

/ *    //get bitrate
    pInfo->m_video_info.bitrate = m_Mdia.minf.stbl.stsd.table->esds.avgBitrate;

    //get duration
    pInfo->m_video_info.duration = ((float)(Ipp64s)(m_Mdia.mdhd.duration) /
                                    (float)m_Mdia.mdhd.time_scale);

      if (pInfo->m_video_info.duration > 0 && !m_headerMPEG4.moov.mvex.total_tracks) {
          pInfo->m_video_info.framerate = (m_Mdia.minf.stbl.stsz.total_entries) /
                                           pInfo->m_video_info.duration;
      } else {
        if (m_headerMPEG4.moov.mvex.total_tracks && m_headerMPEG4.moov.mvex.trex[uiPin]->default_sample_duration) {
            pInfo->m_video_info.framerate = m_Mdia.mdhd.time_scale/
              m_headerMPEG4.moov.mvex.trex[uiPin]->default_sample_duration;
        } else {
          if (m_headerMPEG4.moov.mvex.trex[uiPin] &&
              m_headerMPEG4.moov.mvex.trex[uiPin]->default_sample_duration) {
            pInfo->m_video_info.framerate = m_headerMPEG4.moov.mvex.trex[uiPin]->default_sample_duration;
          }
          //if duration was changed, after parsing of the all stream fragments
          else if (m_pFirstSegmentDuration && m_pFirstSegmentDuration[uiPin] > 0) {
              pInfo->m_video_info.framerate = (Ipp64f)(m_Mdia.minf.stbl.stsz.total_entries) /
                                                  m_pFirstSegmentDuration[uiPin];
            pInfo->m_video_info.framerate *= m_Mdia.mdhd.time_scale;
          } else {
            pInfo->m_video_info.framerate = 0;
          }
        }
    }

    //get stream type
    GetVideoStreamType(m_Mdia.minf.stbl.stsd.table->esds.objectTypeID, &pInfo->m_video_info);
    m_eVideoStreamType = pInfo->m_video_info.stream_type;

    //if any additional video track, then information about them is also filled
    if ( m_nTotalVideoTrack > 1 ) {
      //alloc memory
      if (pInfo->m_video_info_aux) {
          ippsFree(pInfo->m_video_info_aux);
      }
      unsigned int buff_size = (m_nTotalVideoTrack - 1)* sizeof(sVideoStreamInfo);
      pInfo->m_video_info_aux = (sVideoStreamInfo*)ippsMalloc_8u(buff_size);
      if (pInfo->m_video_info_aux) {
          memset(pInfo->m_video_info_aux, 0, buff_size);
      } else {
        vm_debug_trace(VM_DEBUG_PROGRESS, VM_STRING("MP4 Splitter: Not Enough Mem"));
        return UMC_ERR_FAILED;
      }

      for (i = 0; i < m_nTotalVideoTrack - 1; i++) {
        num++;
        uiPin = GetTrackNumber(VIDEO_TRACK, num);

        m_Mdia = m_headerMPEG4.moov.trak[uiPin]->mdia;

        //default values
        pInfo->m_video_info_aux[i].clip_info.width  = (int)m_Mdia.minf.stbl.stsd.table->width;
        pInfo->m_video_info_aux[i].clip_info.height = (int)m_Mdia.minf.stbl.stsd.table->height;

        // find start code VideoObjectLayer and parse width and height
        ParseESDSHeader(m_Mdia.minf.stbl.stsd.table->esds, &pInfo->m_video_info_aux[i], uiPin);

        pInfo->m_video_info_aux[i].bitrate = m_Mdia.minf.stbl.stsd.table->esds.avgBitrate;

        pInfo->m_video_info_aux[i].duration = ((float)(Ipp64s)(m_Mdia.mdhd.duration) /
            (float)m_Mdia.mdhd.time_scale);

        pInfo->m_video_info_aux[i].framerate = m_Mdia.minf.stbl.stsz.total_entries /
            pInfo->m_video_info_aux[i].duration;

        GetVideoStreamType(m_Mdia.minf.stbl.stsd.table->esds.objectTypeID, &pInfo->m_video_info_aux[i]);
      }// for
    }// if* /
  }// if


  return UMC_OK;
}


*/



} // namespace UMC

