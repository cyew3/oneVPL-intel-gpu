//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MP3_AUDIO_DECODER)

#include "umc_mp3_decoder.h"
#include "mp3dec.h"
#include "umc_audio_data.h"
#include "ipps.h"

using namespace UMC;


MP3Decoder::MP3Decoder(void)
{
    state         = NULL;
    stateMemId    = 0;
    m_initialized = 0;
}

MP3Decoder::~MP3Decoder(void)
{
    Close();
}

Status MP3Decoder::Init(BaseCodecParams* init)
{
    Ipp32s size;
    MP3Status status;
    MP3DecoderParams* pMP3DecoderParams;

    // checks or create memory allocator;
    if (BaseCodec::Init(init) != UMC_OK)
        return UMC_ERR_ALLOC;

    pMP3DecoderParams = DynamicCast<MP3DecoderParams, BaseCodecParams>(init);
    if (pMP3DecoderParams)
    {
        m_mc_lfe_filter_off = pMP3DecoderParams->mc_lfe_filter_off;
        m_syncro_mode       = pMP3DecoderParams->synchro_mode;
    }
    else
    {
        m_mc_lfe_filter_off = 0;
        m_syncro_mode       = 0;
    }

    status = mp3decInit(NULL, m_mc_lfe_filter_off, m_syncro_mode, &size);
    if (status != MP3_OK)
        return StatusMP3_2_UMC(status);

    if (m_pMemoryAllocator->Alloc(&stateMemId, size, UMC_ALLOC_PERSISTENT) != UMC_OK)
        return UMC_ERR_ALLOC;

    state = (MP3Dec *)m_pMemoryAllocator->Lock(stateMemId);
    if(!state)
        return UMC_ERR_ALLOC;

    status = mp3decInit(state, m_mc_lfe_filter_off, m_syncro_mode, &size);
    if (status != MP3_OK)
        return StatusMP3_2_UMC(status);

    params.is_valid = 0;
    m_frame_num = 0;
    m_pts_prev = 0;
    m_initialized = 1;

    MemUnlock();

    return StatusMP3_2_UMC(status);
}

Status MP3Decoder::Close(void)
{
    Status status = MemLock();
    if (status != UMC_OK)
        return UMC_OK;

    mp3decClose(state);
    MemUnlock();
    if(state)
    {
        m_pMemoryAllocator->Free(stateMemId);
        state = NULL;
    }

    BaseCodec::Close();

    return UMC_OK;
}

Status MP3Decoder::Reset(void)
{
    m_pts_prev  = 0;
    m_frame_num = 0;

    if (!m_initialized)
        return UMC_ERR_NOT_INITIALIZED;

    Status status = MemLock();
    if (status != UMC_OK)
        return status;

    mp3decReset(state);
    MemUnlock();

    return UMC_OK;
}

Status MP3Decoder::GetFrame(MediaData* in, MediaData* out)
{
    MP3Status resMP3;
    Ipp32s shift = 0;
    Ipp32s frameSize, freq, ch, ch_mask;
    Ipp8u  *in_ptr;
    Ipp16s *out_ptr;
    Ipp32s in_size, out_size;
    Status status;

    if (!in || !in->GetDataPointer() || !out || !out->GetDataPointer())
        return UMC_ERR_NULL_PTR;

    if (!m_initialized)
        return UMC_ERR_NOT_INITIALIZED;

    status = MemLock();
    if (status != UMC_OK)
        return status;

    in_ptr = (Ipp8u *)in->GetDataPointer();
    in_size = (Ipp32s)in->GetDataSize();
    out_ptr = (Ipp16s *)out->GetDataPointer();
    out_size = (Ipp32s)((out->GetBufferSize() - ((Ipp8u*)out_ptr - (Ipp8u*)out->GetBufferPointer())) / sizeof(Ipp16s));
    resMP3 = mp3decGetFrame(in_ptr, in_size, &shift, out_ptr, out_size, state);

    if (shift)
    {
        if ((MP3_OK == resMP3 || MP3_BAD_STREAM == resMP3))
        {
            Ipp64f  pts_start, pts_end;
            in->GetTime(pts_start, pts_end);
            Ipp64f pts_end_snapshot = pts_end;

            mp3decGetFrameSize(&frameSize, state);
            mp3decGetSampleFrequency(&freq, state);
            mp3decGetChannels(&ch, &ch_mask, state);
            if (pts_start < 0)
                pts_start = m_pts_prev;
            m_pts_prev = pts_end =
                pts_start + (Ipp64f)frameSize / (Ipp64f)freq;
            // ELENA: it is no clear why input structure is changed here
            in->MoveDataPointer(shift);
            in->SetTime(pts_start, pts_end_snapshot);

            if (out_size >= frameSize * ch)
            {
                if(MP3_BAD_STREAM == resMP3)
                {
                    resMP3 = MP3_OK;
                    ippsZero_16s(out_ptr, frameSize * ch);
                }
                out->SetDataSize(frameSize * sizeof(Ipp16s) * ch);
                out->SetTime(pts_start, pts_end);
            }
            else
                resMP3 = MP3_NOT_ENOUGH_BUFFER;

            AudioData* pAudio = DynamicCast<AudioData, MediaData>(out);

            if ((MP3_OK == resMP3 && pAudio))
            {
                pAudio->m_iBitPerSample = 16;
                pAudio->m_iChannels = ch;
                pAudio->m_iSampleFrequency = freq;
            }
            m_frame_num++;
        }
        else
            in->MoveDataPointer(shift);
    }

    MemUnlock();

    return StatusMP3_2_UMC(resMP3);
}

Status MP3Decoder::GetInfo(BaseCodecParams* info)
{
    if (!info)
        return UMC_ERR_NULL_PTR;

    AudioDecoderParams *a_info = DynamicCast<AudioDecoderParams, BaseCodecParams>(info);

    if (!m_initialized)
        return UMC_ERR_NOT_INITIALIZED;

    Status status = MemLock();
    if (status != UMC_OK)
        return status;

    mp3decGetInfo(&params, state);
    MemUnlock();

    info->m_SuggestedInputSize = params.m_SuggestedInputSize;
    info->m_SuggestedOutputSize = params.m_SuggestedOutputSize;

    if (!a_info)
        return UMC_OK;

    if (params.is_valid)
    {
        a_info->m_info.bitPerSample= params.m_info_out.bitPerSample;
        a_info->m_info.bitrate = params.m_info_out.bitrate;
        a_info->m_info.channels= params.m_info_out.channels;

        a_info->m_info.sample_frequency = params.m_info_out.sample_frequency;

        switch (params.m_info_in.stream_type)
        {
        case MP1L1_AUD:
            a_info->m_info.stream_type = MP1L1_AUDIO;
            a_info->m_info.iProfile = MP3_PROFILE_MPEG1;
            a_info->m_info.iLevel = MP3_LEVEL_LAYER1;
            break;
        case MP1L2_AUD:
            a_info->m_info.stream_type = MP1L2_AUDIO;
            a_info->m_info.iProfile = MP3_PROFILE_MPEG1;
            a_info->m_info.iLevel = MP3_LEVEL_LAYER2;
            break;
        case MP1L3_AUD:
            a_info->m_info.stream_type = MP1L3_AUDIO;
            a_info->m_info.iProfile = MP3_PROFILE_MPEG1;
            a_info->m_info.iLevel = MP3_LEVEL_LAYER3;
            break;
        case MP2L1_AUD:
            a_info->m_info.stream_type = MP2L1_AUDIO;
            a_info->m_info.iProfile = MP3_PROFILE_MPEG2;
            a_info->m_info.iLevel = MP3_LEVEL_LAYER1;
            break;
        case MP2L2_AUD:
            a_info->m_info.stream_type = MP2L2_AUDIO;
            a_info->m_info.iProfile = MP3_PROFILE_MPEG2;
            a_info->m_info.iLevel = MP3_LEVEL_LAYER2;
            break;
        case MP2L3_AUD:
            a_info->m_info.stream_type = MP2L3_AUDIO;
            a_info->m_info.iProfile = MP3_PROFILE_MPEG2;
            a_info->m_info.iLevel = MP3_LEVEL_LAYER3;
            break;
        default:
            break;
        }

        a_info->m_info.channel_mask = 0;

        if (params.m_info_out.channel_mask & MP3_CHANNEL_STEREO)
            a_info->m_info.channel_mask  |= (CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT);

        if (params.m_info_out.channel_mask & MP3_CHANNEL_CENTER)
            a_info->m_info.channel_mask  |= CHANNEL_FRONT_CENTER;

        if (params.m_info_out.channel_mask & MP3_CHANNEL_LOW_FREQUENCY)
            a_info->m_info.channel_mask  |= CHANNEL_LOW_FREQUENCY;

        if ((params.m_info_out.channel_mask & MP3_CHANNEL_SURROUND_STEREO) ||
            (params.m_info_out.channel_mask & MP3_CHANNEL_SURROUND_STEREO_P2))
            a_info->m_info.channel_mask  |= (CHANNEL_BACK_LEFT | CHANNEL_BACK_RIGHT);

        if (params.m_info_out.channel_mask & MP3_CHANNEL_SURROUND_MONO)
            a_info->m_info.channel_mask |= CHANNEL_BACK_CENTER;

        return UMC_OK;
    }

    return UMC_WRN_INFO_NOT_READY;
}

Status MP3Decoder::GetDuration(Ipp32f* p_duration)
{
    if(!p_duration)
        return UMC_ERR_NULL_PTR;

    if (!m_initialized)
        return UMC_ERR_NOT_INITIALIZED;

    Status status = MemLock();
    if (status != UMC_OK)
        return status;

    mp3decGetDuration(p_duration, state);
    MemUnlock();

    return UMC_OK;
}

Status MP3Decoder::StatusMP3_2_UMC(MP3Status st)
{
    Status res;
    if (st == MP3_OK)
        res = UMC_OK;
    else if (st == MP3_NOT_ENOUGH_DATA)
        res = UMC_ERR_NOT_ENOUGH_DATA;
    else if (st == MP3_BAD_FORMAT)
        res = UMC_ERR_INVALID_STREAM;
    else if (st == MP3_ALLOC)
        res = UMC_ERR_ALLOC;
    else if (st == MP3_BAD_STREAM)
        res = UMC_ERR_INVALID_STREAM;
    else if (st == MP3_NULL_PTR)
        res = UMC_ERR_NULL_PTR;
    else if (st == MP3_NOT_FIND_SYNCWORD)
        res = UMC_ERR_SYNC;
    else if (st == MP3_NOT_ENOUGH_BUFFER)
        res = UMC_ERR_NOT_ENOUGH_BUFFER;
    else if (st == MP3_FAILED_TO_INITIALIZE)
        res = UMC_ERR_INIT;
    else
        res = UMC_ERR_UNSUPPORTED;

    return res;
}

Status MP3Decoder::MemLock()
{
    MP3Dec *pOldState = state;

    if (!m_pMemoryAllocator)
        return UMC_ERR_ALLOC;

    state = (MP3Dec *)m_pMemoryAllocator->Lock(stateMemId);
    if(!state)
        return UMC_ERR_ALLOC;

    if (state != pOldState)
        mp3decUpdateMemMap(state, (Ipp32s)((Ipp8u *)state-(Ipp8u *)pOldState));

    return UMC_OK;
}

Status MP3Decoder::MemUnlock()
{
    if (stateMemId)
    {
        if (m_pMemoryAllocator->Unlock(stateMemId) != UMC_OK)
        return UMC_ERR_ALLOC;
    }
    return UMC_OK;
}

Status MP3Decoder::FrameConstruct(MediaData *in, Ipp32s *outFrameSize, int *bitRate, Ipp32s *outID3HeaderSize, unsigned int * /*p_RawFrameSize*/)
{
    MP3Status res = MP3_OK;
    Ipp8u *inPointer = (Ipp8u *)(in->GetDataPointer());
    Ipp32s inDataSize = (Ipp32s)in->GetDataSize();
    MP3Dec_com state;
    memset(&state,0,sizeof(state));
    *outFrameSize = 0;
   // mp3decInit_com(&state, NULL);
//    Ipp32s decodedBytes;
    state.m_bInit = 0;
    if (!inPointer)
        return StatusMP3_2_UMC(MP3_NULL_PTR);

    res = mp3dec_GetID3Len(inPointer, inDataSize, &state);

    if (res != MP3_OK)
        return StatusMP3_2_UMC(res);

    res = mp3dec_SkipID3(inDataSize, outFrameSize, &state);

    if (res != MP3_OK) {
        return StatusMP3_2_UMC(res);
    } else {
        inDataSize -= *outFrameSize;
        inPointer += *outFrameSize;
        *outID3HeaderSize = *outFrameSize;
    }

    mp3dec_ReceiveBuffer(&(state.m_StreamData), inPointer, inDataSize);

    MP3Status stsMP3 = mp3dec_GetSynch(&state);

    *bitRate = state.header.bitRate;

    //TODO: prevent code copy
    //{   
    //    Ipp32s channels = 0;
    //    IppMP3FrameHeader *header = &(state.header);

    //    //TODO: layer 3 only supported
    //    switch (header->layer) {
    //        case 2: {
    //            if (mp3dec_audio_data_LayerII(&state)) {
    //                //MP3_NOT_FIND_SYNCWORD
    //                return UMC_ERR_SYNC;
    //            }
    //            Ipp32s bits = (Ipp32s)(((state.m_StreamData.pCurrent_dword -
    //                state.start_ptr) << 5) +
    //                state.start_offset -
    //                state.m_StreamData.nBit_offset);
    //            bits = (state.MP3nSlots << 3) - bits;

    //            if (bits >= 35) {
    //                mp3dec_mc_header(&state);
    //                mp3dec_mc_params(&state);
    //            }
    //            channels = state.stereo + state.mc_channel + state.mc_header.lfe;
    //            break;
    //        }
    //        default: {
    //            return UMC_ERR_UNSUPPORTED;
    //        }
    //    }

    //    Ipp32s fs[2][4] = {
    //        { 0, 384, 1152,  576 },
    //        { 0, 384, 1152, 1152 }
    //    };

    //    *p_RawFrameSize = fs[header->id][header->layer] * channels * sizeof(Ipp16s);
    //}    

    *outFrameSize += state.decodedBytes;

    return StatusMP3_2_UMC(stsMP3);
   /* switch(stsMP3)
    {
    case MP3_OK: 
          return UMC_OK;
        break;
    case MP3_BAD_STREAM:
    case MP3_UNSUPPORTED:
    case MP3_NOT_FIND_SYNCWORD:
            return StatusMP3_2_UMC(res);
    case: MP3_NOT_ENOUGH_DATA 
            
            return UMC_ERR_NOT_ENOUGH_DATA;
        }
    default:
        return UMC_ERR_UNSUPPORTED;
        break;
    } */

//    return UMC_OK;
}


namespace UMC
{

Status  DecodeMP3Header( Ipp8u *inPointer, Ipp32s inDataSize, MP3Dec_com *state)
{
    MP3Status res = MP3_OK;
    Ipp32s decodedBytes;
    state->m_bInit = 0;
    if (!inPointer)
        return MP3_NULL_PTR;

    res = mp3dec_GetID3Len(inPointer, inDataSize, state);

    if (res != MP3_OK)
        return res;

    res = mp3dec_SkipID3(inDataSize, &decodedBytes, state);

    if (res != MP3_OK) {
        return res;
    } else {
        inDataSize -= decodedBytes;
        inPointer += decodedBytes;
    }

    mp3dec_ReceiveBuffer(&(state->m_StreamData), inPointer, inDataSize);
    return MP3Decoder::StatusMP3_2_UMC(mp3dec_GetSynch(state));
}
}

#endif
