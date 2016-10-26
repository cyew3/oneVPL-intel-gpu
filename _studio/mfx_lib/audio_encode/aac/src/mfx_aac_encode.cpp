//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_aac_encode.h"
#include "mfx_common.h"
#if defined (MFX_ENABLE_AAC_AUDIO_ENCODE)

#include <math.h>
#include "mfx_common_int.h"
#include "mfx_thread_task.h"
#include "libmfx_core.h"
#include "umc_defs.h"

class MFX_AAC_Encoder_Utility
{
public:
    static mfxStatus Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus FillAudioParam( mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus FillAudioParamByUMC(UMC::AACEncoderParams *in, mfxAudioParam *out);
};


AudioENCODEAAC::AudioENCODEAAC(CommonCORE *core, mfxStatus * sts)
: AudioENCODE()
, m_core()
, m_CommonCore(core)
, m_platform(MFX_PLATFORM_SOFTWARE)
, m_isInit(false)
, m_dTimestampShift(0)
{
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

AudioENCODEAAC::~AudioENCODEAAC(void)
{
    Close();
}

mfxStatus AudioENCODEAAC::Init(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
   
    if (m_isInit)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(par);

    if (CheckAudioParamEncoders(par) != MFX_ERR_NONE)
        return MFX_ERR_INVALID_AUDIO_PARAM;

    m_vPar = *par;

    if (MFX_PLATFORM_SOFTWARE == m_platform)
    {
        m_pAACAudioEncoder.reset(new UMC::AACEncoder());
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    UMC::AACEncoderParams params;

    switch(par->mfx.OutputFormat)
    {
    case MFX_AUDIO_AAC_ADTS:
        params.outputFormat = UMC_AAC_ADTS;
        break;
    case MFX_AUDIO_AAC_ADIF:
        params.outputFormat = UMC_AAC_ADIF;
        break;
    case MFX_AUDIO_AAC_RAW:
        params.outputFormat = UMC_AAC_RAW;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    switch(par->mfx.StereoMode)  
    {
    case MFX_AUDIO_AAC_MONO:
        params.stereo_mode = UMC_AAC_MONO;
        params.m_info.channels = 1;
        break;
    case MFX_AUDIO_AAC_LR_STEREO:
        params.stereo_mode = UMC_AAC_LR_STEREO;
        params.m_info.channels = 2;
        break;
    case MFX_AUDIO_AAC_MS_STEREO:
        params.stereo_mode = UMC_AAC_MS_STEREO;
        params.m_info.channels = 2;
        break;
    case MFX_AUDIO_AAC_JOINT_STEREO:
        params.stereo_mode = UMC_AAC_JOINT_STEREO;
        params.m_info.channels = 2;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    params.ns_mode = 1;
    params.m_info.bitPerSample = par->mfx.BitPerSample;
    params.m_info.stream_type = UMC::AAC_AUDIO;

    switch(par->mfx.CodecProfile)  
    {
    case MFX_PROFILE_AAC_HE:
        params.audioObjectType = AOT_AAC_LC;
        params.auxAudioObjectType = AOT_SBR;
        break;
    case MFX_PROFILE_AAC_LC:
        params.audioObjectType = AOT_AAC_LC;
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    //    params.flag_SBR_support_lev = SBR_ENABLE;
    mInData.SetDataSize(0);
    mOutData.SetDataSize(0);

    params.m_info.sample_frequency = par->mfx.SampleFrequency;
    if (par->mfx.NumChannel == 1) {
        params.stereo_mode = UMC_AAC_MONO;
        params.m_info.channels = 1;
    } 
    //params.StreamInfo.channels = par->mfx.StreamInfo.Channels;
    params.m_info.bitrate = par->mfx.Bitrate;
    params.lpMemoryAllocator = NULL;
  
    UMC::Status sts = m_pAACAudioEncoder->Init(&params);
    if(sts != UMC::UMC_OK)
    {
        return   MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    
    int upSample = m_vPar.mfx.CodecProfile == MFX_PROFILE_AAC_HE ? 2 : 1;
    m_FrameSize  = m_vPar.mfx.NumChannel * sizeof(Ipp16s)*1024* upSample;
    m_divider.reset(new StateDataDivider(m_FrameSize, m_CommonCore));
    m_isInit = true;
    
    return MFX_ERR_NONE;
}



mfxStatus AudioENCODEAAC::Reset(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    m_pAACAudioEncoder->Reset();

    return MFX_ERR_NONE;
}

mfxStatus AudioENCODEAAC::Close(void)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit || !m_pAACAudioEncoder.get())
        return MFX_ERR_NOT_INITIALIZED;

    mOutData.Close();
    mInData.Close();
    m_pAACAudioEncoder->Close();

    m_isInit = false;
    return MFX_ERR_NONE;
}



mfxStatus AudioENCODEAAC::Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out)
{
//    mfxStatus  MFXSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(out);

    return MFX_AAC_Encoder_Utility::Query(core, in, out);
}

mfxStatus AudioENCODEAAC::GetAudioParam(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    mfxStatus  MFXSts = MFX_ERR_NONE;
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    MFX_INTERNAL_CPY(&par->mfx, &m_vPar.mfx, sizeof(mfxInfoMFX));

    UMC::AACEncoderParams params;
    UMC::Status sts = m_pAACAudioEncoder->Init(&params);

    if(sts != UMC::UMC_OK)
    {
        return   MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    MFXSts = MFX_AAC_Encoder_Utility::FillAudioParamByUMC(&params, &m_vPar);
    if (MFXSts != MFX_ERR_NONE)
        return MFXSts;

    MFXSts = MFX_AAC_Encoder_Utility::FillAudioParam(&m_vPar, par);
    if (MFXSts != MFX_ERR_NONE)
        return MFXSts;

    par->Protected = m_vPar.Protected;


    return MFX_ERR_NONE;
}


mfxStatus AudioENCODEAAC::QueryIOSize(AudioCORE *core, mfxAudioParam *par, mfxAudioAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par, request);
    core;

    int upsample = 1;
    if(par->mfx.CodecProfile == MFX_PROFILE_AAC_HE) upsample = 2;
    request->SuggestedInputSize  = par->mfx.NumChannel * sizeof(Ipp16s)*1024* upsample;
    request->SuggestedOutputSize = ((768*par->mfx.NumChannel+9/* ADTS_HEADER */)*sizeof(Ipp8u)+3)&(~3);

    return MFX_ERR_NONE;
}

mfxStatus AudioENCODEAAC::EncodeFrameCheck(mfxAudioFrame *aFrame, mfxBitstream *buffer_out)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    mfxStatus sts;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    sts = aFrame ? CheckAudioFrame(aFrame) : MFX_ERR_NONE;
    if (sts != MFX_ERR_NONE)
        return sts;

    if (aFrame)
    { 
        mfxAudioAllocRequest audioAllocRequest;
        sts = QueryIOSize(m_core, &m_vPar, &audioAllocRequest);
        MFX_CHECK_STS(sts);

        if (buffer_out->MaxLength < audioAllocRequest.SuggestedOutputSize) {
            sts = MFX_ERR_NOT_ENOUGH_BUFFER;
        }
    }

    return sts;
}

mfxStatus AudioENCODEAAC::EncodeFrameCheck(mfxAudioFrame *aFrame,
                                           mfxBitstream *buffer_out,
                                           MFX_ENTRY_POINT *pEntryPoint)
{    
    mfxStatus mfxSts = EncodeFrameCheck(aFrame, buffer_out);
    if (mfxSts)
        return mfxSts;

    mfxSts = m_divider->PutPair(std::pair<mfxAudioFrame*, mfxBitstream*>(aFrame, buffer_out));

    if ((MFX_ERR_NONE == mfxSts) || (MFX_ERR_MORE_BITSTREAM == mfxSts)) // It can be useful to run threads right after first frame receive
    {
        ThreadAudioEncodeTaskInfo * info = new ThreadAudioEncodeTaskInfo();
        info->out = buffer_out;
        info->in = aFrame;
        if (aFrame) {
            m_dTimestampShift = (mfxI32)(( (float)m_FrameSize - (float)aFrame->DataLength ) 
                / ((float)m_vPar.mfx.SampleFrequency * (float)m_vPar.mfx.NumChannel * 2.0f) * 90000.0f);
            buffer_out->TimeStamp = aFrame->TimeStamp + m_dTimestampShift;
            buffer_out->DecodeTimeStamp = buffer_out->TimeStamp;
        }
        //finally queue task
        
        pEntryPoint->pRoutine = &AACENCODERoutine;
        pEntryPoint->pCompleteProc = &AACCompleteProc;
        pEntryPoint->pState = this;
        pEntryPoint->requiredNumThreads = 1; // need only one thread
        pEntryPoint->pParam = info;
    }
    return mfxSts;
}

mfxStatus AudioENCODEAAC::AACENCODERoutine(void *pState, void *pParam,
                                          mfxU32 threadNumber, mfxU32 callNumber)
{
    pParam;
    callNumber;
    threadNumber;
    
    AudioENCODEAAC &obj = *((AudioENCODEAAC *) pState);

    MFX::AutoTimer timer("EncodeFrame");

    mfxBitstream* bitstream = obj.m_divider->GetNextBitstream();
    mfxU8* buffer = (mfxU8*)obj.m_divider->GetBuffer();
    if (!buffer || !bitstream)
        return MFX_ERR_NONE;

    obj.mInData.SetBufferPointer(buffer, obj.m_FrameSize);
    obj.mInData.SetDataSize(obj.mInData.GetBufferSize());
    obj.mOutData.SetBufferPointer(bitstream->Data + bitstream->DataOffset + bitstream->DataLength, 
        bitstream->MaxLength - (bitstream->DataOffset + bitstream->DataLength));
    UMC::Status sts = obj.m_pAACAudioEncoder.get()->GetFrame(&obj.mInData, &obj.mOutData);
    if (sts)
        return MFX_ERR_UNKNOWN;
    bitstream->DataLength += (mfxU32)obj.mOutData.GetDataSize();

    return MFX_ERR_NONE;

} // mfxStatus AudioENCODEAAC::AACECODERoutine(void *pState, void *pParam,

mfxStatus AudioENCODEAAC::AACCompleteProc(void *pState, void *pParam,
                                          mfxStatus taskRes)
{
    taskRes;
    pParam;
    AudioENCODEAAC &obj = *((AudioENCODEAAC *) pState);

    if (MFX_PLATFORM_SOFTWARE == obj.m_platform)
    {
        ThreadAudioEncodeTaskInfo *pTask = (ThreadAudioEncodeTaskInfo *) pParam;
        if (pTask)
            delete pTask;

        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

//    return MFX_ERR_NONE;
}

// protected methods
mfxStatus AudioENCODEAAC::CopyBitstream(mfxAudioFrame& bs, const mfxU8* ptr, mfxU32 bytes)
{
    if (bs.DataLength + bytes > bs.MaxLength)
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    ippsCopy_8u(ptr, bs.Data, bytes);
    bs.DataLength = bytes;
    return MFX_ERR_NONE;
}

void AudioENCODEAAC::MoveBitstreamData(mfxAudioFrame& bs, mfxU32 offset)
{
    ippsMove_8u(bs.Data + offset, bs.Data, bs.DataLength - offset);
    bs.DataLength -= offset;
} 

mfxStatus MFX_AAC_Encoder_Utility::FillAudioParam( mfxAudioParam *in, mfxAudioParam *out)
{
    out->AsyncDepth = in->AsyncDepth;
    out->Protected = in->Protected;
    out->mfx.CodecId = out->mfx.CodecId;
    out->mfx.CodecLevel = in->mfx.CodecLevel;
    out->mfx.CodecProfile = in->mfx.CodecProfile;
    out->mfx.Bitrate = in->mfx.Bitrate;
    out->mfx.SampleFrequency = in->mfx.SampleFrequency;
    out->mfx.NumChannel = in->mfx.NumChannel;
    out->mfx.BitPerSample = in->mfx.BitPerSample;
    out->mfx.FlagPSSupportLev = in->mfx.FlagPSSupportLev;
    out->mfx.Layer = in->mfx.Layer;
    return MFX_ERR_NONE;
}

mfxStatus MFX_AAC_Encoder_Utility::FillAudioParamByUMC(UMC::AACEncoderParams *in, mfxAudioParam *out)
{
    out->mfx.BitPerSample = (mfxU16)in->m_info.bitPerSample;
    out->mfx.Bitrate = (mfxU32)in->m_info.bitrate;
    out->mfx.NumChannel = (mfxU16)in->m_info.channels;
    out->mfx.SampleFrequency = (mfxU32)in->m_info.sample_frequency;
    return MFX_ERR_NONE;
}

mfxStatus MFX_AAC_Encoder_Utility::Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (in == out)
    {
        mfxAudioParam in1;
        MFX_INTERNAL_CPY(&in1, in, sizeof(mfxAudioParam));
        return Query(core, &in1, out);
    }

    MFX_INTERNAL_CPY(&out->mfx, &in->mfx, sizeof(mfxAudioInfoMFX));

    if (in)
    {
        // save bitrate from input params
        out->mfx.Bitrate = in->mfx.Bitrate;
        
        // save sample frequency from input params
        out->mfx.SampleFrequency = in->mfx.SampleFrequency;

        if (in->mfx.CodecId == MFX_CODEC_AAC)
            out->mfx.CodecId = in->mfx.CodecId;


        switch(in->mfx.CodecProfile)
        {
        case MFX_PROFILE_AAC_LC :
        case MFX_PROFILE_AAC_HE :
            out->mfx.CodecProfile = in->mfx.CodecProfile;
            break;
        default:
            out->mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
        };

        switch (in->mfx.CodecLevel)
        {
        case MFX_LEVEL_UNKNOWN:
            out->mfx.CodecLevel = in->mfx.CodecLevel;
            break;
        }

        mfxU16 outputFormat = in->mfx.OutputFormat;
        if (outputFormat == MFX_AUDIO_AAC_ADTS || 
            outputFormat == MFX_AUDIO_AAC_RAW || 
            outputFormat == MFX_AUDIO_AAC_ADIF 
            )
        {
            out->mfx.OutputFormat = in->mfx.OutputFormat;
        }
        else
        {
            if(outputFormat == 0)
            {
                out->mfx.OutputFormat = MFX_AUDIO_AAC_RAW;
            }
            else
            {
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        mfxU16 stereoMode = in->mfx.StereoMode;
        if (stereoMode == MFX_AUDIO_AAC_MONO || 
            stereoMode == MFX_AUDIO_AAC_LR_STEREO ||
            stereoMode == MFX_AUDIO_AAC_MS_STEREO ||
            stereoMode == MFX_AUDIO_AAC_JOINT_STEREO 
            )
        {
            //num channels not specified
            switch (in->mfx.NumChannel)
            {
                case 0 : {
                    if (stereoMode == MFX_AUDIO_AAC_MONO) {
                        out->mfx.NumChannel = 1;
                    }
                    else {
                        out->mfx.NumChannel = 2;
                    }
                    break ;
                }
                case 1: {
                    out->mfx.NumChannel = 1;
                    out->mfx.StereoMode = MFX_AUDIO_AAC_MONO;
                    break;
                }
                case 2: {
                    out->mfx.NumChannel = 2;
                    //TODO: looks query shouldn't do this, setting default strereo mode
                    if (stereoMode == MFX_AUDIO_AAC_MONO) {
                        out->mfx.StereoMode = MFX_AUDIO_AAC_JOINT_STEREO;
                    }
                }
            } 
        }
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.BitPerSample != 16 && in->mfx.BitPerSample != 0) {
            sts = MFX_ERR_UNSUPPORTED;
        } else {
            out->mfx.BitPerSample = 16;
        }
    }
    else
    {
        out->mfx.CodecId = MFX_CODEC_AAC;
        out->mfx.CodecProfile = 0;
        out->mfx.CodecLevel = 0;
        out->AsyncDepth = 1;
    }        

    return sts;
}

bool AudioENCODEAAC::AudioFramesCollector::UpdateBuffer() {
    mGuard.Lock();
    size_t CurrentFrameLength = 0;
    int nPopFront = 0;
    for (std::list<mfxAudioFrame*>::iterator it = list.begin() ; it != list.end() ; ++it ) {
        size_t nFreeBytesInBuffer = buffer.size() - CurrentFrameLength;
        if (!(*it)) {
            //padding for the last frame
            memset(&buffer.front() + CurrentFrameLength, 0, buffer.size() - CurrentFrameLength);
            nPopFront++;
            break;
        }
        size_t nFrameSizeWithoutOffset = (*it)->DataLength - offset;
        if (nFrameSizeWithoutOffset <= nFreeBytesInBuffer) {
            MFX_INTERNAL_CPY(&buffer.front() + CurrentFrameLength, (*it)->Data + offset, nFrameSizeWithoutOffset);
            offset = 0;
            CurrentFrameLength += nFrameSizeWithoutOffset % buffer.size();
            mCore.DecreasePureReference((*it)->Locked);
            nPopFront++;
        } else {
            MFX_INTERNAL_CPY(&buffer.front() + CurrentFrameLength, (*it)->Data + offset, nFreeBytesInBuffer);
            offset += nFreeBytesInBuffer;
            break;
        }            
    }
    for (int i = 0; i < nPopFront; i++)
        list.pop_front();
    mGuard.Unlock();
    //TODO: errors handling
    return true;
}

#endif // MFX_ENABLE_AAC_AUDIO_ENCODE
