/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2005-2016 Intel Corporation. All Rights Reserved.
//
*/

#include <ipps.h>
#include "umc_stream_parser.h"
#include "umc_frame_constructor.h"

#define PARSER_CHECK_INIT CHECK_OBJ_INIT(m_pDataReader)
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace UMC
{

StreamParserParams::StreamParserParams(void)
{
    m_SystemType = UNDEF_STREAM;
    m_pDataReader = NULL;
}

StreamParserParams::~StreamParserParams()
{
}

StreamParser::StreamParser()
{
    m_pDataReader = NULL;
    m_pPacket = NULL;
}

StreamParser::~StreamParser()
{
    Close();
}

Status StreamParser::Init(StreamParserParams &init)
{
    if (m_pDataReader) // already initialized
        return UMC_ERR_FAILED;

    if (!init.m_pDataReader)
        return UMC_ERR_NULL_PTR;

    ippsZero_8u((Ipp8u *)m_pInfo, MAX_TRACK * sizeof(void *));
    m_ParserState = HEADER;
    m_dDuration = -1.0;
    m_uiTracks = 0;

    m_SystemType = init.m_SystemType;
    m_pDataReader = init.m_pDataReader;
    m_uiSourceSize = init.m_pDataReader->GetSize();

    if (!m_pPacket) m_pPacket = new Packet;
    return UMC_OK;
}

Status StreamParser::Close(void)
{
    PARSER_CHECK_INIT;
    Ipp32s i;
    for (i = 0; i < (Ipp32s)m_uiTracks; i++)
    {
        if (m_pInfo[i])
        {
            m_pInfo[i]->ReleaseAll();
            delete m_pInfo[i];
            m_pInfo[i] = NULL;
        }
    }
    if (m_pPacket)
    {
        delete m_pPacket;
        m_pPacket = NULL;
    }
    m_pDataReader = NULL;
    return UMC_OK;
}

Status StreamParser::CheckNextData(MediaData *pData, Ipp32u* pTrack)
{
    PARSER_CHECK_INIT;
    if (!pData || !pTrack)
        return UMC_ERR_NULL_PTR;

    if (0 == m_uiTracks)
    {
        m_uiTracks = 1;
        if (!m_pInfo[0])
            m_pInfo[0] = new Mpeg2TrackInfo;

        switch (m_SystemType)
        {
        case MPEG1_PURE_VIDEO_STREAM:
            m_pInfo[0]->m_Type = TRACK_MPEG1V; break;
        case MPEG2_PURE_VIDEO_STREAM:
            m_pInfo[0]->m_Type = TRACK_MPEG2V; break;
        case MPEG4_PURE_VIDEO_STREAM:
            m_pInfo[0]->m_Type = TRACK_MPEG4V; break;
        case H261_PURE_VIDEO_STREAM:
            m_pInfo[0]->m_Type = TRACK_H261; break;
        case H263_PURE_VIDEO_STREAM:
            m_pInfo[0]->m_Type = TRACK_H263; break;
        case H264_PURE_VIDEO_STREAM:
            m_pInfo[0]->m_Type = TRACK_H264; break;
        case MPEG1_PURE_AUDIO_STREAM:
        case MPEG2_PURE_AUDIO_STREAM:
        case MPEGx_PURE_AUDIO_STREAM:
            m_pInfo[0]->m_Type = TRACK_MPEGA; break;
        case MJPEG_PURE_VIDEO_STREAM:
            m_pInfo[0]->m_Type = TRACK_MJPEG; break;
        default:
            m_pInfo[0]->m_Type = TRACK_UNKNOWN; break;
        }

        Status err = m_pInfo[0]->Alloc();
        if (UMC_OK != err)
            return err;

        if (m_pInfo[0]->m_Type & TRACK_ANY_AUDIO)
        {
            if (m_dDuration < 0.0 && (m_pInfo[0]->m_Type & TRACK_MPEGA))
                EstimateMPEGAudioDuration();
            ((AudioStreamInfo *)m_pInfo[0]->m_pStreamInfo)->duration = m_dDuration;
        }
    }

    m_ParserState = PAYLOAD;
    pTrack[0] = 0;
    m_pPacket->uiSize = PURE_STREAMS_PORTION;
    m_pPacket->uiAbsPos = m_pDataReader->GetPosition();
    PacketToMediaData(*m_pPacket, *pData);
    return UMC_OK;
}

Status StreamParser::GetNextData(MediaData *pData, Ipp32u *pTrack)
{
    PARSER_CHECK_INIT;
    if (END_OF_STREAM == m_ParserState)
        return UMC_ERR_END_OF_STREAM;
    if (!pData || !pData->GetDataPointer() || !pTrack)
        return UMC_ERR_NULL_PTR;

    Status umcRes = UMC_OK;
    if (HEADER == m_ParserState)
    {
        umcRes = CheckNextData(pData, pTrack);
        if (UMC_OK != umcRes)
            return umcRes;
    }

    if (m_pPacket->uiSize > pData->GetBufferSize())
        return UMC_ERR_NOT_ENOUGH_BUFFER;

    // move to the beginning of payload
    umcRes = m_pDataReader->MovePosition((Ipp64u)m_pPacket->iBufOffset);
    if (UMC_ERR_END_OF_STREAM == umcRes)
    {
        m_ParserState = END_OF_STREAM;
        return umcRes;
    }

    umcRes = m_pDataReader->GetData(pData->GetDataPointer(), &m_pPacket->uiSize);
    if (UMC_ERR_END_OF_STREAM == umcRes)
    {
        m_ParserState = END_OF_STREAM;
        if (0 == m_pPacket->uiSize)
            return umcRes;
        umcRes = UMC_OK;
    }

    *pTrack = GetTrackByPidOrCreateNew(m_pPacket->iPid, NULL);
#ifdef _BIG_ENDIAN_
    if (*pTrack >= 0 && TRACK_PCM == m_pInfo[*pTrack]->m_Type)
        ippsSwapBytes_16u_I((Ipp16u *)pData->GetDataPointer(), m_pPacket->uiSize / 2);
#else
    if (*pTrack >= 0 && TRACK_LPCM == m_pInfo[*pTrack]->m_Type)
        ippsSwapBytes_16u_I((Ipp16u *)pData->GetDataPointer(), m_pPacket->uiSize / 2);
#endif // _BIG_ENDIAN_

    m_ParserState = HEADER;
    PacketToMediaData(*m_pPacket, *pData);
    return umcRes;
}

Ipp32u StreamParser::GetNumberOfTracks(void)
{
    return m_pDataReader ? m_uiTracks : 0;
}

Mpeg2TrackInfo *StreamParser::GetTrackInfo(Ipp32u uiTrack)
{
    return m_pDataReader && uiTrack < m_uiTracks ? m_pInfo[uiTrack] : NULL;
}

SystemStreamType StreamParser::GetSystemStreamType(void)
{
    return m_pDataReader ? m_SystemType : UNDEF_STREAM;
}

void StreamParser::Reset(void)
{
    m_ParserState = HEADER;
    if (m_pPacket) m_pPacket->Reset();
}

Status StreamParser::SetPosition(Ipp64u bytePos)
{
    PARSER_CHECK_INIT;
    Reset();
    return (m_uiSourceSize > 0 || 0 == bytePos) ? m_pDataReader->SetPosition(IPP_MIN(bytePos, m_uiSourceSize)) : UMC_OK;
}

Ipp64u StreamParser::GetPosition(void)
{
    return m_pDataReader ? m_pDataReader->GetPosition() : 0;
}

Ipp64u StreamParser::GetSize(void)
{
    return m_pDataReader ? m_uiSourceSize : 0;
}

Status StreamParser::MoveToNextHeader()
{
    PARSER_CHECK_INIT;
    if (END_OF_STREAM == m_ParserState)
        return UMC_ERR_END_OF_STREAM;
    else if (HEADER == m_ParserState)
        return UMC_OK;
    Status umcRes = m_pDataReader->MovePosition(m_pPacket->iBufOffset + m_pPacket->uiSize);
    Reset();
    return umcRes;
}

Ipp64f StreamParser::GetDuration(void)
{
    return m_pDataReader ? m_dDuration : -1.0;
}

Ipp32s StreamParser::GetTrackByPid(Ipp32u uiPid)
{
    return GetTrackByPidOrCreateNew((Ipp32s)uiPid, NULL);
}

Ipp32s StreamParser::GetTrackByPidOrCreateNew(Ipp32s iPid, bool *pIsNew)
{
    Ipp32u i;
    for (i = 0; i < m_uiTracks; i++)
        if (m_pInfo[i]->m_PID == (Ipp32u)iPid)
            break;

    if (i < m_uiTracks)
        return (Ipp32s)i;
    else if (!pIsNew || m_uiTracks >= MAX_TRACK)
        return -1;

    *pIsNew = true;
    m_uiTracks;
    if (!m_pInfo[m_uiTracks])
        m_pInfo[m_uiTracks] = new Mpeg2TrackInfo;
    m_pInfo[m_uiTracks]->m_PID = iPid;
    m_uiTracks++;
    return m_uiTracks - 1;
}

void StreamParser::PacketToMediaData(FCSample &packet, MediaData &data)
{
    if (NULL == data.GetBufferPointer())
    {
        data.SetBufferPointer((Ipp8u *)1, packet.uiSize);
        data.SetDataSize(packet.uiSize);
    }
    else
    {
        data.SetBufferPointer((Ipp8u *)data.GetBufferPointer(), packet.uiSize);
        data.SetDataSize(packet.uiSize);
    }

    data.SetTime(packet.dPTS, packet.dDTS);
    if (DynamicCast<SplMediaData>(&data))
        ((SplMediaData *)&data)->SetAbsPos(packet.uiAbsPos);
}

Status ParseInitialObjectDescriptor(Mpeg2TsPmt &pmt, Ipp8u *pParam, Ipp32s buflen);
Status Mpeg2PesParser::ParsePmtInfo(Mpeg2TsPmt &pmt)
{
    Ipp8u *pPtr = NULL;
    Ipp32u uiTag, uiLen;
    if (pmt.pProgInfo && pmt.uiProgInfoLen)
    {
        DescriptorNavigator descrNav(pmt.pProgInfo, pmt.uiProgInfoLen);
        while (NULL != (pPtr = descrNav.GetNextDescriptor(&uiTag, &uiLen)))
        {
            if (DESC_IOD == uiTag && uiLen > 2)
                ParseInitialObjectDescriptor(pmt, pPtr + 2, uiLen - 2);
        }
    }

    Ipp32u i;
    for (i = 0; i < pmt.uiESs; i++)
    {
        bool bIsNew = false;
        Ipp32s iTrack = GetTrackByPidOrCreateNew(pmt.pESs[i].uiPid, &bIsNew);
        if (iTrack < 0 || !bIsNew)
            continue;

        m_pInfo[iTrack]->m_uiProgNum = pmt.uiProgInd;
        AudioStreamType aType = UNDEF_AUDIO;
        if (0x01 == pmt.pESs[i].uiType)
            m_pInfo[iTrack]->m_Type = TRACK_MPEG1V;
        else if (0x02 == pmt.pESs[i].uiType)
            m_pInfo[iTrack]->m_Type = TRACK_MPEG2V;
        else if (0x03 == pmt.pESs[i].uiType || 0x04 == pmt.pESs[i].uiType)
        {
            m_pInfo[iTrack]->m_Type = TRACK_MPEGA;
            aType = (0x03 == pmt.pESs[i].uiType) ? MPEG1_AUDIO : MPEG2_AUDIO;
        }
        else if (0x06 == pmt.pESs[i].uiType)
        { // we should wait for decriptor to determine type
            m_pInfo[iTrack]->m_Type = TRACK_UNKNOWN;
        }
        else if (0x0F == pmt.pESs[i].uiType)
            m_pInfo[iTrack]->m_Type = TRACK_AAC;
        else if (0x10 == pmt.pESs[i].uiType)
            m_pInfo[iTrack]->m_Type = TRACK_MPEG4V;
        else if (0x11 == pmt.pESs[i].uiType)
        {
            m_pInfo[iTrack]->m_Type = TRACK_AAC;
            aType = AAC_MPEG4_STREAM;
        }
        else if (0x1A == pmt.pESs[i].uiType || 0x1B == pmt.pESs[i].uiType)
            m_pInfo[iTrack]->m_Type = TRACK_H264;
        else if (0x81 == pmt.pESs[i].uiType)
            m_pInfo[iTrack]->m_Type = TRACK_AC3;
        else if (0x83 == pmt.pESs[i].uiType)
            m_pInfo[iTrack]->m_Type = TRACK_LPCM;
        else
            m_pInfo[iTrack]->m_Type = TRACK_UNKNOWN;

        m_pInfo[iTrack]->ReleaseStreamInfo();
        Status umcRes = m_pInfo[iTrack]->Alloc();
        if (UMC_OK != umcRes)
            return umcRes;

        if (m_pInfo[iTrack]->m_Type & TRACK_ANY_AUDIO)
        {
            if (UNDEF_AUDIO == aType)
                aType = (AudioStreamType)ConvertTrackType(m_pInfo[iTrack]->m_Type);
            ((AudioStreamInfo *)m_pInfo[iTrack]->m_pStreamInfo)->stream_type = aType;
        }

        if (pmt.pESs[i].pEsInfo && pmt.pESs[i].uiEsInfoLen)
        {
            // parse descriptors if present
            DescriptorNavigator descrNav(pmt.pESs[i].pEsInfo, pmt.pESs[i].uiEsInfoLen);
            while (NULL != (pPtr = descrNav.GetNextDescriptor(&uiTag, &uiLen)))
            {
                if (DESC_VIDEO == uiTag && m_pInfo[iTrack]->m_Type & TRACK_ANY_VIDEO)
                {
                    VideoStreamInfo *pVideoSpec = (VideoStreamInfo *)m_pInfo[iTrack]->m_pStreamInfo;
                    Ipp8u frame_rate_code = (pPtr[0] >> 3) & 0x0F;
                    Ipp8u MPEG_1_only_flag = (pPtr[0] >> 2) & 0x01;
                    pVideoSpec->framerate = Mpeg2FrameConstructor::FrameRate[frame_rate_code < 9 ? frame_rate_code : 0];
                    pVideoSpec->stream_type = MPEG_1_only_flag ? MPEG1_VIDEO : MPEG2_VIDEO;
                    m_pInfo[iTrack]->m_Type = MPEG_1_only_flag ? TRACK_MPEG1V : TRACK_MPEG2V;
                }
                else if (DESC_AUDIO == uiTag && m_pInfo[iTrack]->m_Type & TRACK_ANY_AUDIO)
                {
                    AudioStreamInfo *pAudioSpec = (AudioStreamInfo *)m_pInfo[iTrack]->m_pStreamInfo;
                    Ipp8u id = (pPtr[0] >> 6) & 0x01;
                    Ipp8u layer = 4 - ((pPtr[0] >> 4) & 0x03);
                    const size_t maxLayerIndex = sizeof(AudioFrameConstructor::MpegAStreamType[0])/sizeof(AudioFrameConstructor::MpegAStreamType[0][0]);
                    if (layer <= maxLayerIndex)
                    {
                        pAudioSpec->stream_type = AudioFrameConstructor::MpegAStreamType[id][layer - 1];
                    }
                    m_pInfo[iTrack]->m_Type = TRACK_MPEGA;
                }
                else if ((DESC_AC3 == uiTag || DESC_ENH_AC3 == uiTag) && pmt.pESs[i].uiType == 0x06)
                {
                    m_pInfo[iTrack]->m_Type = TRACK_AC3;
                    m_pInfo[iTrack]->Alloc();
                    ((AudioStreamInfo *)m_pInfo[iTrack]->m_pStreamInfo)->stream_type = AC3_AUDIO;
                    ((AudioStreamInfo *)m_pInfo[iTrack]->m_pStreamInfo)->streamPID = m_pInfo[iTrack]->m_PID;
                }
                else if ((DESC_TXT == uiTag || DESC_VBI_TXT == uiTag) && pmt.pESs[i].uiType == 0x06)
                {
                    if (uiLen < 5) continue;
                    m_pInfo[iTrack]->m_Type = TRACK_VBI_TXT;
                    TeletextStreamInfo *pTxtSpec = (TeletextStreamInfo *)m_pInfo[iTrack]->m_pStreamInfo;
                    pTxtSpec->szLanguage[0] = pPtr[0];
                    pTxtSpec->szLanguage[1] = pPtr[1];
                    pTxtSpec->szLanguage[2] = pPtr[2];
                    pTxtSpec->szLanguage[3] = 0;
                    pTxtSpec->uiType = (pPtr[3] >> 3) & 0x1f;
                    pTxtSpec->uiMagazineNumber = pPtr[3] & 0x07;
                    pTxtSpec->uiPageNumber = pPtr[4];
                }
                else if (DESC_VBI == uiTag && pmt.pESs[i].uiType == 0x06)
                {
                    m_pInfo[iTrack]->m_Type = TRACK_ANY_VBI;
                }
                else if (DESC_FMC == uiTag)
                {
                    if (uiLen < 2) continue;
                    Ipp16u esId = GET_16U(pPtr);
                    Ipp32u i;
                    for (i = 0; i < pmt.uiESs; i++)
                    {
                        if (pmt.pESs[i].pESDSs && pmt.pESs[i].pESDSs->uiEsId == esId)
                        {
                            UMC_NEW(m_pInfo[iTrack]->m_pDecSpecInfo, MediaData);
                            m_pInfo[iTrack]->m_pDecSpecInfo->SetBufferPointer(
                                pmt.pESs[i].pESDSs->pDecSpecInfo, pmt.pESs[i].pESDSs->uiDecSpecInfoLen);
                            m_pInfo[iTrack]->m_pDecSpecInfo->SetDataSize(pmt.pESs[i].pESDSs->uiDecSpecInfoLen);

                            if (m_pInfo[iTrack]->m_Type & TRACK_ANY_VIDEO)
                                ((VideoStreamInfo *)m_pInfo[iTrack]->m_pStreamInfo)->bitrate = pmt.pESs[i].pESDSs->avgBitrate;
                            else if (m_pInfo[iTrack]->m_Type & TRACK_ANY_AUDIO)
                                ((AudioStreamInfo *)m_pInfo[iTrack]->m_pStreamInfo)->bitrate = pmt.pESs[i].pESDSs->avgBitrate;
                            break;
                        }
                    }
                }
            }
        }

        if (TRACK_UNKNOWN == m_pInfo[iTrack]->m_Type)
        { // oops, seems it's unsupported type
            m_uiTracks--;
            continue;
        }
    }

    return UMC_OK;
}

namespace
{
    unsigned int multiCharToInt(const char val[4])
    {
        return (val[0] << 24) + (val[1] << 16) + (val[2] << 8) + val[3];
    }
}

Status WaveParser::CheckNextData(MediaData *pData, Ipp32u *pTrack)
{
    PARSER_CHECK_INIT;
    if (!pData || !pTrack)
        return UMC_ERR_NULL_PTR;

    if (0 == m_uiTracks)
    {
        Status err;
        Ipp32u longCode, fmtSize;
        Ipp16u shortCode;

        m_uiTracks = 1;
        m_pInfo[0] = new Mpeg2TrackInfo;
        if (!m_pInfo[0])
            return UMC_ERR_ALLOC;

        err = m_pDataReader->Get32uSwap(&longCode); // 'RIFF'
        if (UMC_OK != err || longCode != multiCharToInt("RIFF")) return UMC_ERR_INVALID_STREAM;
        err = m_pDataReader->Get32uNoSwap(&longCode); // size
        if (UMC_OK != err) return err;
        err = m_pDataReader->Get32uSwap(&longCode); // 'WAVE'
        if (UMC_OK != err || longCode != multiCharToInt("WAVE")) return UMC_ERR_INVALID_STREAM;

        err = m_pDataReader->Get32uSwap(&longCode); // 'fmt '
        if (UMC_OK != err || longCode != multiCharToInt("fmt ")) return UMC_ERR_INVALID_STREAM;
        err = m_pDataReader->Get32uNoSwap(&fmtSize); // size
        if (UMC_OK != err) return err;

        err = m_pDataReader->Get16uNoSwap(&shortCode); // AudioFormat
        if (UMC_OK != err) return err;
        fmtSize -= 2;

        if (WAVE_FORMAT_PCM == shortCode)
            m_pInfo[0]->m_Type = TRACK_PCM;
        else if (WAVE_FORMAT_MPEGLAYER3 == shortCode)
            m_pInfo[0]->m_Type = TRACK_MPEGA;

        err = m_pInfo[0]->Alloc();
        if (UMC_OK != err)
            return err;

        AudioStreamInfo *pASI = (AudioStreamInfo *)m_pInfo[0]->m_pStreamInfo;
        err = m_pDataReader->Get16uNoSwap(&shortCode); // NumChannels
        if (UMC_OK != err) return err;
        pASI->channels = shortCode;
        fmtSize -= 2;

        err = m_pDataReader->Get32uNoSwap(&longCode); // SampleRate
        if (UMC_OK != err) return err;
        pASI->sample_frequency = longCode;
        fmtSize -= 4;

        err = m_pDataReader->Get32uNoSwap(&longCode); // ByteRate
        if (UMC_OK != err) return err;
        pASI->bitrate = 8 * longCode;
        fmtSize -= 4;

        err = m_pDataReader->Get16uNoSwap(&shortCode); // BlockAlign
        if (UMC_OK != err) return err;
        fmtSize -= 2;

        err = m_pDataReader->Get16uNoSwap(&shortCode); // BitsPerSample
        if (UMC_OK != err) return err;
        pASI->bitPerSample = shortCode;
        fmtSize -= 2;

        err = m_pDataReader->MovePosition(fmtSize); // skip fmt header
        if (UMC_OK != err) return err;

        err = m_pDataReader->Check32u(&longCode, 0);
        while (UMC_OK == err && multiCharToInt("data") != longCode)
        {
            err = m_pDataReader->Get32uSwap(&longCode);
            err = m_pDataReader->Get32uNoSwap(&longCode);
            err = m_pDataReader->MovePosition(longCode);
            err = m_pDataReader->Check32u(&longCode, 0);
        }

        err = m_pDataReader->Get32uSwap(&longCode); // 'data'
        if (UMC_OK != err) return err;
        err = m_pDataReader->Get32uNoSwap(&longCode); // size of 'data'
        if (UMC_OK != err) return err;

        if (m_dDuration < 0.0)
        {
            if (m_pInfo[0]->m_Type & TRACK_MPEGA)
            {
                EstimateMPEGAudioDuration();
            }
            else if (TRACK_PCM == m_pInfo[0]->m_Type)
            {
                if (m_uiSourceSize && pASI->bitrate)
                    m_dDuration = (Ipp64f)(Ipp64s)m_uiSourceSize / (pASI->bitrate / 8);
            }
        }

        pASI->duration = m_dDuration;
    }

    m_ParserState = PAYLOAD;
    pTrack[0] = 0;
    m_pPacket->uiSize = PURE_STREAMS_PORTION;
    m_pPacket->uiAbsPos = m_pDataReader->GetPosition();
    PacketToMediaData(*m_pPacket, *pData);
    return UMC_OK;
}

Status Mpeg2PesParser::ParsePesHeader(Packet &packet, Ipp32s &iPos, bool bWrapped)
{
    Ipp32s iInnerPos = 0;
    // we save 9 bytes for start_code, pes_len, flags and pes_header_len
    // 256 bytes are maximum for pes_packet_len and 1 is for first byte of private_1
    Ipp8u pesPacket[9 + 256 + 1];
    CACHE_N_BYTES(&pesPacket[0], 9, iPos);

    if (pesPacket[0] != 0 || pesPacket[1] != 0 || pesPacket[2] != 1)
        return UMC_ERR_INVALID_STREAM;
    if (!IS_PES_PACKET(pesPacket[3]))
        return UMC_ERR_INVALID_STREAM;

    if (!bWrapped)
        packet.iPid = pesPacket[3];
    Ipp32s PES_packet_len = GET_16U(&pesPacket[4]);

    packet.dPTS = packet.dDTS = -1.0;
    if (bWrapped || MPEG2_PROGRAMM_STREAM == m_SystemType)
    {
        Ipp32u PTS_DTS_flag = (pesPacket[iInnerPos + 7] >> 6) & 0x03;
        Ipp32s PES_header_data_length = pesPacket[iInnerPos + 8];

        if (0 == PES_packet_len && !bWrapped)
            return UMC_ERR_INVALID_STREAM;
        if (PES_packet_len > 0 && PES_packet_len < PES_header_data_length + 3)
            return UMC_ERR_INVALID_STREAM;

        CACHE_N_BYTES(&pesPacket[9], PES_header_data_length + 1, iPos + 9);
        if (PTS_DTS_flag & 2)
        {
            packet.dPTS = GetTimeStampFromPes(&pesPacket[iInnerPos + 9]);
            if (3 == PTS_DTS_flag)
                packet.dDTS = GetTimeStampFromPes(&pesPacket[iInnerPos + 9 + 5]);
        }

        iInnerPos += 9 + PES_header_data_length;
        if (!bWrapped)
            packet.uiSize = PES_packet_len - 3 - PES_header_data_length;

        // Private_stream_1 may contain more then one elementary stream
        if (0xBD == packet.iPid && !bWrapped)
            packet.iPid = pesPacket[iInnerPos];

    }
    else if (MPEG1_PROGRAMM_STREAM == m_SystemType)
    {
        Ipp32s i;
        // 34 bytes are maximum that needed for mpeg1 pes header, 9 have been already read
        CACHE_N_BYTES(&pesPacket[9], 34 - 9, iPos + 9);
        iInnerPos += 6;

        // skip stuffing bytes, they are start from '1' and not more than 16
        for (i = 0; 0x80 == (pesPacket[iInnerPos] & 0x80) && i < 16; i++, iInnerPos++);

        if ((pesPacket[iInnerPos] & 0xc0) == 0x40)
            iInnerPos += 2;

        if ((pesPacket[iInnerPos] & 0xf0) == 0x20)
        {
            packet.dPTS = GetTimeStampFromPes(&pesPacket[iInnerPos]);
            iInnerPos += 5;
        }
        else if ((pesPacket[iInnerPos] & 0xf0) == 0x30)
        {
            packet.dPTS = GetTimeStampFromPes(&pesPacket[iInnerPos]);
            iInnerPos += 5;
            packet.dDTS = GetTimeStampFromPes(&pesPacket[iInnerPos]);
            iInnerPos += 5;
        }
        else if (0x0f == pesPacket[iInnerPos])
            iInnerPos += 1;
        else
            return UMC_ERR_INVALID_STREAM;

        if (iInnerPos > PES_packet_len + 6)
            return UMC_ERR_INVALID_STREAM;

        packet.uiSize = PES_packet_len + 6 - iInnerPos;
    }

    iPos += iInnerPos;
    packet.iBufOffset = iPos;
    return UMC_OK;
}

Status StreamParser::EstimateMPEGAudioDuration(void)
{
    Ipp8u buf[BUF_LEN];
    Ipp32u ndata;
    Ipp32s frame_len;
    Ipp32u offset, offset_add, offset_save, i;
    Ipp32s frame_counter;
    Ipp32s next_header;
    Ipp32s layer, samplingFreq, bitRate, emphasis, id, mpg25, MP3Header, paddingBit;
    Ipp32s layer_ref = 0, samplingFreq_ref = 0, id_ref = 0, mpg25_ref = 0;
    Ipp32s free_format = 0;
    Ipp64u iPosition = 0;

    Status status = UMC_OK;
    if (!(m_pInfo[0]->m_Type & TRACK_MPEGA))
        return UMC_OK;

    iPosition = m_pDataReader->GetPosition();

    frame_counter = 0;

    ndata = BUF_LEN;
    offset = BUF_LEN;
    offset_add = 0;

    for(;;)
    {
        offset_save = ndata - offset;
        if (offset_save)
        {
            ndata -= offset_save;
            for (i = 0; i < offset_save; i++)
                buf[i] = buf[offset + i];
            status = m_pDataReader->GetData(buf + offset_save, &ndata);
        }
        else
            status = m_pDataReader->GetData(buf, &ndata);

        if (ndata == 0)
            break;

        ndata += offset_save;
        offset = offset_add;
        offset_add = 0;

        while(offset < ndata - 3)
        {
            if (buf[offset] != 0xff || ((buf[offset + 1]) & 0xe0) != 0xe0)
            {
                offset++;
                continue;
            }

            MP3Header = (buf[offset] << 24) | (buf[offset+1] << 16) | (buf[offset+2] << 8) | buf[offset+3];

            layer = MPEGA_HDR_LAYER(MP3Header);
            samplingFreq = MPEGA_HDR_SAMPLINGFREQ(MP3Header);
            bitRate = MPEGA_HDR_BITRADEINDEX(MP3Header);
            emphasis = MPEGA_HDR_EMPH(MP3Header);
            id = MPEGA_HDR_VERSION(MP3Header);
            mpg25 = ((MP3Header >> 20) & 1) ? 0 : 2;
            paddingBit = MPEGA_HDR_PADDING(MP3Header);

            if (frame_counter)
            {
                if ((samplingFreq != samplingFreq_ref) ||
                    (layer != layer_ref) ||
                    (id != id_ref) ||
                    (mpg25 != mpg25_ref) ||
                    (bitRate == 15) ||
                    (emphasis == 2) ||
                    (free_format != (bitRate == 0)))
                {
                    offset++;
                    continue;
                }

                if (free_format)
                {
                    offset ++;
                    if (bitRate)
                        continue;
                    frame_counter++;
                }
                else
                {
                    frame_len = 0;
                    if (layer == 4)
                        return UMC::UMC_ERR_INVALID_STREAM; // should not appear, but technically possible
                    else if (layer == 3)
                        frame_len = 72000 * (id + 1);
                    else if (layer == 2)
                        frame_len = 72000 * 2;
                    else if (layer == 1)
                        frame_len = 12000;

                    const size_t maxIdValue = sizeof(AudioFrameConstructor::MpegAFrequency)/sizeof(AudioFrameConstructor::MpegAFrequency[0]);
                    if (static_cast<size_t>(id + mpg25) >= maxIdValue)
                    {
                        return UMC::UMC_ERR_INVALID_STREAM; // should not appear, but technically possible
                    }

                    frame_len = frame_len * AudioFrameConstructor::MpegABitrate[id][layer - 1][bitRate] /
                    AudioFrameConstructor::MpegAFrequency[id + mpg25][samplingFreq] + paddingBit;

                    if (layer == 1)
                        frame_len *= 4;

                    frame_counter++;
                    offset += frame_len;
                }
            }
            else
            {
                if ((samplingFreq == 3) ||
                    (layer == 4) ||
                    (bitRate == 15) ||
#ifdef FREE_FORMAT_PROHIBIT
                    (bitRate == 0) ||
#endif
                    (emphasis == 2) ||
                    (((MP3Header >> 19) & 3) == 1))
                {
                    offset++;
                    continue;
                }

                if (bitRate == 0)
                {
                    samplingFreq_ref = samplingFreq;
                    layer_ref = layer;
                    id_ref = id;
                    mpg25_ref = mpg25;
                    free_format = 1;
                    frame_counter++;
                    offset++;
                }
                else
                {
                    frame_len = 0;
                    if (layer == 4)
                        return UMC::UMC_ERR_INVALID_STREAM; // should not appear, but technically possible
                    else if (layer == 3)
                        frame_len = 72000 * (id + 1);
                    else if (layer == 2)
                        frame_len = 72000 * 2;
                    else if (layer == 1)
                        frame_len = 12000;

                    const size_t maxIdValue = sizeof(AudioFrameConstructor::MpegAFrequency)/sizeof(AudioFrameConstructor::MpegAFrequency[0]);
                    if (static_cast<size_t>(id + mpg25) >= maxIdValue)
                    {
                        return UMC::UMC_ERR_INVALID_STREAM; // should not appear, but technically possible
                    }

                    frame_len = frame_len * AudioFrameConstructor::MpegABitrate[id][layer - 1][bitRate] /
                        AudioFrameConstructor::MpegAFrequency[id + mpg25][samplingFreq] + paddingBit;

                    if (layer == 1)
                        frame_len *= 4;

                    if (offset + frame_len + 3 >= ndata)
                        break;
                    next_header = (buf[offset + frame_len + 0] << 16) |
                                  (buf[offset + frame_len + 1] << 8) |
                                  (buf[offset + frame_len + 2]);

                    if ((next_header ^ (MP3Header >> 8)) & 0xfffe0c)
                    {
                        offset++;
                        continue;
                    }
                    frame_counter++;
                    offset += frame_len;

                    samplingFreq_ref = samplingFreq;
                    layer_ref = layer;
                    id_ref = id;
                    mpg25_ref = mpg25;
                }
            }
            if (offset >= ndata)
            {
                offset_add = offset - ndata;
                offset = ndata;
                break;
            }
        }
    }
    status = m_pDataReader->SetPosition(iPosition);
    if (status != UMC::UMC_OK)
        return status;

    m_dDuration = frame_counter * AudioFrameConstructor::MpegAFramesize[id_ref][layer_ref] /
        AudioFrameConstructor::MpegAFrequency[id_ref + mpg25_ref][samplingFreq_ref];
    return status;
}

DescriptorNavigator::DescriptorNavigator(Ipp8u *pPtr, Ipp32u uiLen)
{
    m_pPtr = pPtr;
    m_uiLen = uiLen;
}

Ipp8u *DescriptorNavigator::GetNextDescriptor(Ipp32u *pTag, Ipp32u *pLen)
{
    if (m_uiLen < 2)
        return NULL;

    *pTag = m_pPtr[0];
    *pLen = m_pPtr[1];
    if (m_uiLen < 2 + *pLen)
        return NULL;

    Ipp8u *ptr = m_pPtr + 2;
    m_pPtr += 2 + *pLen;
    m_uiLen -= 2 + *pLen;
    return ptr;
}

Ipp32u GetSizeOfInstance(BitstreamReader &bs)
{
    Ipp32u size = 0, n = 0;
    Ipp8u nextByte;

    do {
        nextByte = (Ipp8u)bs.CopyBit();
        size = (size << 7) | (bs.GetBits(8) & 0x7f);
        n++;
    } while (nextByte && n < 4);

    return size;
}

Status ParseInitialObjectDescriptor(Mpeg2TsPmt &pmt, Ipp8u *pParam, Ipp32s iBufLen)
{
    BitstreamReader bs;
    bs.Init(pParam);
    // check InitialObjectDescriptorTag
    if (0x02 != bs.GetBits(8))
        return UMC_ERR_FAILED;
    Ipp32u uiIodSize = GetSizeOfInstance(bs);
    if (bs.Stream() + uiIodSize > pParam + iBufLen)
        return UMC_ERR_FAILED;
    bs.SkipBits(10); //skip ObjectDescriptorID
    if (bs.GetBits(1)) //URL_flag
        return UMC_ERR_FAILED;
    bs.SkipBits(5); // skip includeInlineProfileLevelFlag and reserved bits
    bs.SkipBits(32); // skip OD-, scene-, audio-, visualProfileLevelIndication
    bs.SkipBits(8); // graphicsProfileLevelIndication

    while (0x03 == bs.GetBits(8))
    {
        Ipp32u uiEsdSize = GetSizeOfInstance(bs);
        Ipp8u *pEsd = bs.Stream();
        if (pEsd + uiEsdSize > pParam + iBufLen)
            return UMC_ERR_FAILED;
        Ipp16u ES_ID = (Ipp16u)bs.GetBits(16);

        Ipp32u i;
        for (i = 0; i < pmt.uiESs; i++)
        {
            Ipp8u *ptr;
            Ipp32u tag = 0, len;

            // find FMC descriptor in loop
            DescriptorNavigator nav(pmt.pESs[i].pEsInfo, pmt.pESs[i].uiEsInfoLen);
            while (NULL != (ptr = nav.GetNextDescriptor(&tag, &len)))
                if (DESC_FMC == tag)
                    break;

            // check if descriptor was found and if it is of needed ES
            if (ptr && DESC_FMC == tag && len >= 2 && GET_16U(ptr) == ES_ID)
                break;
        }
        if (i < pmt.uiESs)
        {
            Ipp32u streamDependenceFlag = bs.GetBit();
            Ipp32u URL_Flag = bs.GetBit();
            Ipp32u OCRstreamFlag = bs.GetBit();
            bs.SkipBits(5); // streamPriority
            if (streamDependenceFlag)
                bs.SkipBits(16);
            if (URL_Flag)
                bs.SkipBits(8 * bs.GetBits(8));
            if (OCRstreamFlag)
                bs.SkipBits(16);
            if (0x04 != bs.GetBits(8)) // DecoderConfigDescrTag
                return UMC_ERR_FAILED;
            /*Ipp32u uiDcdSize =*/ GetSizeOfInstance(bs);
            bs.SkipBits(16); // skip objectTypeIndication, streamType, upStream, reserved
            bs.SkipBits(24); // skip bufferSizeDB
            bs.SkipBits(32); // skip maxBitrate
            Ipp32u avgBitrate = bs.GetBits(32);
            if (bs.Stream() > pEsd + uiEsdSize)
                return UMC_ERR_FAILED;
            if (0x05 != bs.GetBits(8)) // DecSpecificInfoTag
                return UMC_ERR_FAILED;
            Ipp32u uiDsiSize = GetSizeOfInstance(bs);
            Ipp8u *pDecSpecificInfo = bs.Stream();
            bs.SkipBits(8 * uiDsiSize);
            if (0x06 != bs.GetBits(8)) // SLConfigDescriptorTag
                return UMC_ERR_FAILED;
            pmt.pESs[i].pESDSs = new ESDescriptor;
            pmt.pESs[i].pESDSs->uiEsId = ES_ID;
            pmt.pESs[i].pESDSs->avgBitrate = avgBitrate;
            pmt.pESs[i].pESDSs->uiPredefinedSLConfig = (Ipp8u)bs.GetBits(8);
            pmt.pESs[i].pESDSs->uiDecSpecInfoLen = uiDsiSize;
            pmt.pESs[i].pESDSs->pDecSpecInfo = pDecSpecificInfo;
        }
    }

    return UMC_OK;
}

}
