//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_asf_spl.h"
#include "ipps.h"


namespace UMC
{

Splitter *CreateASFSplitter() { return (new ASFSplitter()); }

ASFSplitter::ASFSplitter()
{
    m_pDataReader = NULL;
    m_pDataObject = NULL;
    m_ppFBuffer = NULL;
    m_ppLockedFrame = NULL;
    m_pES2PIDTbl = NULL;
    m_pInfo = &m_info;
    m_bFlagStop = true;
    m_pReadDataPacketThread = NULL;
}

ASFSplitter::~ASFSplitter()
{
    Close();
}

void GetVideoStreamType(VideoStreamInfo *pVideoInfo, Ipp32u compresID)
{
  switch (compresID) {
    case 0x33564D57:    // "WMV3"
        pVideoInfo->stream_type = WMV_VIDEO;
        pVideoInfo->stream_subtype = WMV3_VIDEO;
        break;
    case 0x31435657:    // "WVC1"
        pVideoInfo->stream_type = WMV_VIDEO;
        //pVideoInfo->stream_subtype = VC1_VIDEO_VC1;
        pVideoInfo->stream_subtype = WVC1_VIDEO;
        break;
    case 0x3334504D:    // "MP43"
    case 0x5334504D:    // "MP4S"
    case 0x7334706D:    // "mp4s"
    case 0x3253344D:    // "M4S2"
    case 0x3273346D:    // "m4s2"
        pVideoInfo->stream_type = MPEG4_VIDEO;
        pVideoInfo->stream_subtype = MPEG4_VIDEO_QTIME;
        break;
    case 0x34363248:
        pVideoInfo->stream_type = H264_VIDEO;
        pVideoInfo->stream_subtype = UNDEF_VIDEO_SUBTYPE;
        break;
    default:
        pVideoInfo->stream_type = UNDEF_VIDEO;
        pVideoInfo->stream_subtype = UNDEF_VIDEO_SUBTYPE;
  }
}

Status ASFSplitter::FillAudioInfo(Ipp32u nTrack)
{
    AudioStreamInfo *pAudioInfo = (AudioStreamInfo *)(m_pInfo->m_ppTrackInfo[nTrack]->m_pStreamInfo);
    Status umcRes = UMC_OK;
    asf_StreamPropObject *pStrPropObj = m_pHeaderObject->ppStreamPropObject[nTrack];
    asf_AudioMediaInfo *pAudioSpecData = pStrPropObj->typeSpecData.pAudioSpecData;

    switch (pAudioSpecData->formatTag)
    {
        case 0x0001:
            pAudioInfo->stream_type = PCM_AUDIO;
            pAudioInfo->stream_subtype = UNDEF_AUDIO_SUBTYPE;
            break;
        case 0x0161:  /*** Windows Media Audio. This format is valid for versions 2 through 9 ***/
        case 0x0162:  /*** Windows Media Audio 9 Professional ***/
        case 0x0163:  /*** Windows Media Audio 9 Lossless ***/
        default:
            pAudioInfo->stream_type = UNDEF_AUDIO;
            pAudioInfo->stream_subtype = UNDEF_AUDIO_SUBTYPE;
    }

    pAudioInfo->channels = pAudioSpecData->numChannels;
    pAudioInfo->sample_frequency = pAudioSpecData->sampleRate;
    pAudioInfo->bitrate = pAudioSpecData->avgBytesPerSec * 8;
    pAudioInfo->bitPerSample = pAudioSpecData->bitsPerSample;

    if (pAudioSpecData->codecSpecDataSize)
    {
      MediaData *pDecSpecInfo = new MediaData(pAudioSpecData->codecSpecDataSize);
      UMC_CHECK_PTR(pDecSpecInfo)
      memcpy_s(pDecSpecInfo->GetDataPointer(), pAudioSpecData->codecSpecDataSize,
               pAudioSpecData->pCodecSpecData, pAudioSpecData->codecSpecDataSize);
      pDecSpecInfo->SetDataSize(pAudioSpecData->codecSpecDataSize);
      pDecSpecInfo->SetTime(0, 0);
      m_pInfo->m_ppTrackInfo[nTrack]->m_pDecSpecInfo = pDecSpecInfo;
    }

    pAudioInfo->streamPID = m_pInfo->m_ppTrackInfo[nTrack]->m_PID;

    return umcRes;
}

Status ASFSplitter::FillVideoInfo(Ipp32u nTrack)
{
    VideoStreamInfo *pVideoInfo = (VideoStreamInfo *)(m_pInfo->m_ppTrackInfo[nTrack]->m_pStreamInfo);
    Status umcRes = UMC_OK;
    asf_StreamPropObject *pStrPropObj = m_pHeaderObject->ppStreamPropObject[nTrack];
    asf_VideoMediaInfo *pVideoSpecData = (asf_VideoMediaInfo *)pStrPropObj->typeSpecData.pAudioSpecData;

    pVideoInfo->clip_info.width = pVideoSpecData->width;
    pVideoInfo->clip_info.height = pVideoSpecData->height;
    pVideoInfo->aspect_ratio_width = pVideoSpecData->FormatData.hrzPixelsPerMeter;
    pVideoInfo->aspect_ratio_height = pVideoSpecData->FormatData.vertPixelsPerMeter;
    pVideoInfo->color_format = YV12;

    //get stream type
    GetVideoStreamType(pVideoInfo, pVideoSpecData->FormatData.compresID);

    switch (pVideoInfo->stream_type) {
        case MPEG4_VIDEO:
          m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_MPEG4V;
          break;
        case WMV_VIDEO:
        case VC1_VIDEO:
            m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_WMV;
            break;
        default:
            m_pInfo->m_ppTrackInfo[nTrack]->m_Type = TRACK_UNKNOWN;
    }

    Ipp32u len = pVideoSpecData->FormatData.formatDataSize - VIDEO_SPEC_DATA_LEN;
    if (len)
    {
        Ipp32u shift = 0;
        MediaData *pDecSpecInfo = new MediaData(len);
        UMC_CHECK_PTR(pDecSpecInfo)

        if (pVideoInfo->stream_subtype == WVC1_VIDEO)
        {
            shift = 1;
            len -= shift;
        }
        memcpy_s(pDecSpecInfo->GetDataPointer(), len, pVideoSpecData->FormatData.pCodecSpecData + shift, len);
        pDecSpecInfo->SetDataSize(len);
        pDecSpecInfo->SetTime(0, 0);
        m_pInfo->m_ppTrackInfo[nTrack]->m_pDecSpecInfo = pDecSpecInfo;
    }

    pVideoInfo->streamPID = m_pInfo->m_ppTrackInfo[nTrack]->m_PID;

    return umcRes;
}

Ipp32u ASFSplitter::ReadDataPacketThreadCallback(void* pParam)
{
    VM_ASSERT(NULL != pParam);
    ASFSplitter* pThis = (ASFSplitter*)pParam;
//    Ipp32u nPack = 0;
    Ipp32u i = 0;
    Status umcRes = UMC_OK;

//printf("%d packets declared\n", pThis->m_pDataObject->totalDataPackets);
    while (umcRes == UMC_OK && !pThis->m_bFlagStop)
    {
        umcRes = pThis->ReadDataPacket();
/*
        if (umcRes == UMC_OK)
            nPack++;
        else
printf("not OK\n");
*/

    }
//printf("%d packets read\n", nPack);

    for (i = 0; i < pThis->m_pInfo->m_nOfTracks; i++)
    {
//printf("set flag for buffer %d\n", i);
       pThis->m_ppFBuffer[i]->UnLockInputBuffer(NULL, UMC_ERR_END_OF_STREAM);
    }

    return 0;
}

Status ASFSplitter::CleanInternalObjects()
{
    Ipp32u iES = 0;

    for (iES = 0; iES < m_pInfo->m_nOfTracks; iES++) {
        if (m_ppFBuffer)
            UMC_DELETE(m_ppFBuffer[iES]);
        if (m_ppLockedFrame)
            UMC_DELETE(m_ppLockedFrame[iES]);
        if (m_pInfo->m_ppTrackInfo)
        {
            UMC_DELETE(m_pInfo->m_ppTrackInfo[iES]->m_pStreamInfo);
            UMC_DELETE(m_pInfo->m_ppTrackInfo[iES]);
        }
    }
    delete[] m_pES2PIDTbl;
    delete[] m_ppLockedFrame;
    delete[] m_ppFBuffer;
    delete[] m_pInfo->m_ppTrackInfo;

    return UMC_OK;
}

Status ASFSplitter::Init(SplitterParams& init)
{
    Ipp32u iES, nAudio = 0, nVideo = 0, i = 0;
    Status umcRes = UMC_OK;

    if (init.m_pDataReader == NULL)
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    m_pDataReader = init.m_pDataReader;
    m_pInfo->m_splitter_flags = init.m_lFlags;
    m_pInfo->m_SystemType = ASF_STREAM;

    Ipp32u numOfStreams = 0;
    umcRes = CountNumberOfStreams(numOfStreams);
    UMC_CHECK_STATUS(umcRes)

    m_pInfo->m_nOfTracks = numOfStreams;

    umcRes = ReadHeaderObject();
    UMC_CHECK_STATUS(umcRes)

    if (m_pHeaderObject->pFPropObject->flags & 0x01) {
        m_pInfo->m_dDuration = 0; /*** broadcast ***/
    } else {
        m_pInfo->m_dDuration = (Ipp64f)m_pHeaderObject->pFPropObject->playDuration * 1E-7;
    }

    m_pInfo->m_ppTrackInfo = new TrackInfo*[m_pInfo->m_nOfTracks];
    memset(m_pInfo->m_ppTrackInfo, 0, m_pInfo->m_nOfTracks*sizeof(TrackInfo*));
    m_ppFBuffer = new ASFFrameBuffer*[m_pInfo->m_nOfTracks];
    memset(m_ppFBuffer, 0, m_pInfo->m_nOfTracks*sizeof(ASFFrameBuffer*));
    m_ppLockedFrame = new asf_LockedFrame*[m_pInfo->m_nOfTracks];
    memset(m_ppLockedFrame, 0, m_pInfo->m_nOfTracks*sizeof(asf_LockedFrame*));
    m_pES2PIDTbl = new Ipp32u[m_pInfo->m_nOfTracks];
    memset(m_pES2PIDTbl, 0, m_pInfo->m_nOfTracks*sizeof(Ipp32u));

    for (iES = 0; iES < m_pInfo->m_nOfTracks; iES++) {
        MediaBufferParams mParams;
        UMC_NEW(m_pInfo->m_ppTrackInfo[iES], TrackInfo);
        m_pInfo->m_ppTrackInfo[iES]->m_PID = m_pHeaderObject->ppStreamPropObject[iES]->flags & 0x7f;
        m_pES2PIDTbl[iES] = m_pInfo->m_ppTrackInfo[iES]->m_PID;

        if (m_pHeaderObject->ppStreamPropObject[iES]->streamType == ASF_Audio_Media) {
            nAudio++;
            UMC_NEW(m_pInfo->m_ppTrackInfo[iES]->m_pStreamInfo, AudioStreamInfo);
            umcRes = FillAudioInfo(iES);
            UMC_CHECK_STATUS(umcRes)
            if ((m_pInfo->m_dRate == 1) && (m_pInfo->m_splitter_flags & AUDIO_SPLITTER) &&
                (m_pInfo->m_ppTrackInfo[iES]->m_Type != TRACK_UNKNOWN)) {
                m_pInfo->m_ppTrackInfo[iES]->m_isSelected = 1;
            }
        } else if (m_pHeaderObject->ppStreamPropObject[iES]->streamType == ASF_Video_Media) {
            nVideo++;
            UMC_NEW(m_pInfo->m_ppTrackInfo[iES]->m_pStreamInfo, VideoStreamInfo);
            umcRes = FillVideoInfo(iES);
            UMC_CHECK_STATUS(umcRes)
            if (m_pInfo->m_splitter_flags & VIDEO_SPLITTER) {
                m_pInfo->m_ppTrackInfo[iES]->m_isSelected = 1;
            }
        } else {
            continue; // skip unsupported stream
        }

        UMC_NEW(m_ppLockedFrame[iES], asf_LockedFrame);

        UMC_NEW(m_ppFBuffer[iES], ASFFrameBuffer);
        mParams.m_numberOfFrames = ASF_NUMBER_OF_FRAMES;
        mParams.m_prefInputBufferSize =  mParams.m_prefOutputBufferSize = ASF_PREF_FRAME_SIZE;
        if (m_pHeaderObject->pHeaderExtObject->headerExtDataSize)
        {
            for (i = 0; i < ASF_MAX_NUM_OF_STREAMS; i++)
            {
                if(m_pHeaderObject->pHeaderExtObject->pHeaderExtData->ppExtStreamPropObject[i])
                {
                    if (m_pHeaderObject->pHeaderExtObject->pHeaderExtData->ppExtStreamPropObject[i]->streamNum ==
                        m_pInfo->m_ppTrackInfo[iES]->m_PID)
                    {
                        if (m_pHeaderObject->pHeaderExtObject->pHeaderExtData->ppExtStreamPropObject[i]->maxObjSize > 0)
                            mParams.m_prefInputBufferSize =  mParams.m_prefOutputBufferSize =
                            m_pHeaderObject->pHeaderExtObject->pHeaderExtData->ppExtStreamPropObject[i]->maxObjSize;
                        break;
                    }
                }

            }
        }
        UMC_CALL(m_ppFBuffer[iES]->Init(&mParams));
    }

    if (nVideo == 0) {
        m_pInfo->m_splitter_flags &= ~VIDEO_SPLITTER;
    }
    if (nAudio == 0) {
        m_pInfo->m_splitter_flags &= ~AUDIO_SPLITTER;
    }

    umcRes = ReadDataObject();

    return umcRes;
}

Status ASFSplitter::Close()
{

    m_bFlagStop = true;
    if (vm_thread_is_valid(m_pReadDataPacketThread)) {
        vm_thread_wait(m_pReadDataPacketThread);
        vm_thread_close(m_pReadDataPacketThread);
        vm_thread_set_invalid(m_pReadDataPacketThread);
    }

    UMC_DELETE(m_pDataObject);
    CleanInternalObjects();
    CleanHeaderObject();

    return UMC_OK;
}


Status ASFSplitter::GetInfo(SplitterInfo** ppInfo)
{
    ppInfo[0] = &m_info;
    return UMC_OK;
}

Status ASFSplitter::Run()
{
    int res = 0;

    if (!m_bFlagStop)
        return UMC_OK;

    m_pReadDataPacketThread = new vm_thread;
    vm_thread_set_invalid(m_pReadDataPacketThread);
    /*** start thread which read data and fill buffers ***/
    res = vm_thread_create(m_pReadDataPacketThread, (vm_thread_callback)ReadDataPacketThreadCallback, (void *)this);
    if (res != 1) {
      return UMC_ERR_FAILED;
    }

    m_bFlagStop = false;

    return UMC_OK;
}

Status ASFSplitter::GetNextData(MediaData* data, Ipp32u nTrack)
{
    Status umcRes = UMC_OK;

    if (nTrack > m_pInfo->m_nOfTracks)
        return UMC_ERR_FAILED;

    if (!m_ppFBuffer[nTrack] || !m_pInfo->m_ppTrackInfo[nTrack]->m_isSelected)
        return UMC_ERR_FAILED;

    if (m_ppLockedFrame[nTrack]->m_IsLockedFlag)
    {
        m_ppLockedFrame[nTrack]->m_pLockedFrame->SetDataSize(0);
        m_ppFBuffer[nTrack]->UnLockOutputBuffer(m_ppLockedFrame[nTrack]->m_pLockedFrame);
        m_ppLockedFrame[nTrack]->m_IsLockedFlag = false;
    }

    umcRes = m_ppFBuffer[nTrack]->LockOutputBuffer(data);
    if (umcRes == UMC_OK)
    {
        *(m_ppLockedFrame[nTrack]->m_pLockedFrame) = *data;
        m_ppLockedFrame[nTrack]->m_IsLockedFlag = true;
    }

    return umcRes;
}

Status ASFSplitter::CheckNextData(MediaData* data, Ipp32u nTrack)
{
    Status umcRes = UMC_OK;

    if (nTrack > m_pInfo->m_nOfTracks)
        return UMC_ERR_FAILED;

    if (!m_ppFBuffer[nTrack] || !m_pInfo->m_ppTrackInfo[nTrack]->m_isSelected)
        return UMC_ERR_FAILED;

    if (m_ppLockedFrame[nTrack]->m_IsLockedFlag)
    {
        m_ppLockedFrame[nTrack]->m_pLockedFrame->SetDataSize(0);
        m_ppFBuffer[nTrack]->UnLockOutputBuffer(m_ppLockedFrame[nTrack]->m_pLockedFrame);
        m_ppLockedFrame[nTrack]->m_IsLockedFlag = false;
    }

    umcRes = m_ppFBuffer[nTrack]->LockOutputBuffer(data);
    if (umcRes == UMC_OK)
    {
        m_ppFBuffer[nTrack]->UnLockOutputBuffer(data);
    }

    return umcRes;
}

Status ASFSplitter::SetTimePosition(Ipp64f /*position*/)
{
  return UMC_ERR_NOT_IMPLEMENTED;
}

Status ASFSplitter::GetTimePosition(Ipp64f& /*position*/)
{
  return UMC_ERR_NOT_IMPLEMENTED;
}

Status ASFSplitter::SetRate(Ipp64f /*rate*/)
{
  return UMC_ERR_NOT_IMPLEMENTED;
}

Status ASFSplitter::EnableTrack(Ipp32u /*nTrack*/, Ipp32s /*iState*/)
{
  return UMC_ERR_NOT_IMPLEMENTED;
}

} // namespace UMC

