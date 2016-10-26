//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_aac_dec_decode.h"
#include "mfx_common.h"
#if defined (MFX_ENABLE_AAC_AUDIO_DECODE)

#include "mfx_common_int.h"
#include "mfx_thread_task.h"
#include "umc_defs.h"

static Ipp32s aac_sampling_frequency_table[] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000,
    22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

class MFX_AAC_Utility
{
public:
    static mfxStatus Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus FillAudioParam( mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus FillAudioParamByUMC(UMC::AACDecoderParams *in, mfxAudioParam *out);
};


AudioDECODEAAC::AudioDECODEAAC(AudioCORE *core, mfxStatus * sts)
    : AudioDECODE()
    , m_core(core)
    , m_platform(MFX_PLATFORM_SOFTWARE)
    , m_isInit(false)
    , m_nUncompFrameSize()
{
    m_frame.Data = NULL;
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

AudioDECODEAAC::~AudioDECODEAAC(void)
{
    Close();
}

mfxStatus AudioDECODEAAC::Init(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (m_isInit)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(par);


    if (CheckAudioParamDecoders(par) != MFX_ERR_NONE)
        return MFX_ERR_INVALID_AUDIO_PARAM;

    m_vPar = *par;

    if (MFX_PLATFORM_SOFTWARE == m_platform)
    {
        m_pAACAudioDecoder.reset(new UMC::AACDecoder());
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }
    // allocate memory
    //
    Ipp32u MaxLength = 768 * CH_MAX;

    memset(&m_frame, 0, sizeof(mfxBitstream));
    m_frame.MaxLength = MaxLength;
    if (m_frame.Data)
    {
        delete[] m_frame.Data;
        m_frame.Data = NULL;
    }
    m_frame.Data = new mfxU8[MaxLength];
    m_frame.DataLength = 0;
    m_frame.DataOffset = 0;

    UMC::AACDecoderParams params;
    params.ModeDecodeHEAACprofile = HEAAC_HQ_MODE;

    switch(par->mfx.FlagPSSupportLev)  {
case MFX_AUDIO_AAC_PS_DISABLE:
    params.flag_PS_support_lev = PS_DISABLE;
    break;
case MFX_AUDIO_AAC_PS_PARSER:
    params.flag_PS_support_lev = PS_PARSER;
    break;
case MFX_AUDIO_AAC_PS_ENABLE_BL:
    params.flag_PS_support_lev = PS_ENABLE_BL;
    break;
case MFX_AUDIO_AAC_PS_ENABLE_UR:
    params.flag_PS_support_lev = PS_ENABLE_UR;
    break;
default:
    return MFX_ERR_UNSUPPORTED;
    }
    params.ModeDwnsmplHEAACprofile = HEAAC_DWNSMPL_OFF;
    params.flag_SBR_support_lev = SBR_ENABLE;

    mInData.SetDataSize(0);
    mOutData.SetDataSize(0);
    params.m_pData = NULL;
    params.m_info.stream_type = UMC::AAC_FMT_UNDEF;

    if(par->mfx.AACHeaderData != 0 && par->mfx.AACHeaderDataSize > 0)
    {
        params.m_pData = new UMC::MediaData();
        params.m_pData->SetBufferPointer(&(par->mfx.AACHeaderData[0]),par->mfx.AACHeaderDataSize);
        params.m_pData->SetDataSize(par->mfx.AACHeaderDataSize);
        // Check header type
        sAdif_header audio_adif_data;
        sAdts_fixed_header audio_adts_fixed_data;
        sBitsreamBuffer BS;
        GET_INIT_BITSTREAM(&BS, par->mfx.AACHeaderData)

            BS.nDataLen = (Ipp32s)par->mfx.AACHeaderDataSize;
        bs_save(&BS);
        if (UMC::DecodeADIFSHeader(&audio_adif_data, &BS) != UMC::UMC_OK)
        {
            bs_restore(&BS);
            if(UMC::DecodeADTSfixedHeader(&audio_adts_fixed_data, &BS) != UMC::UMC_OK)
            {
                params.m_info.stream_type = UMC::AAC_MPEG4_STREAM;
                m_inputFormat = MFX_AUDIO_AAC_ESDS;
            }
            else
            {
                m_inputFormat = MFX_AUDIO_AAC_ADTS;
            }
        }
        else
        {
            m_inputFormat = MFX_AUDIO_AAC_ADIF;
        }
    }

     UMC::Status sts = m_pAACAudioDecoder->Init(&params);
    if(sts != UMC::UMC_OK)
    {
        delete params.m_pData;
        return   ConvertStatusUmc2Mfx(sts);
    }
    delete params.m_pData;
    params.m_pData = NULL;
    m_isInit = true;

    return MFX_ERR_NONE;
}



mfxStatus AudioDECODEAAC::Reset(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);


    /*
    if (!MFX_Utility::CheckVideoParam(par))
    return MFX_ERR_INVALID_VIDEO_PARAM;  */

    /*if (!IsSameVideoParam(par, &m_vPar))
    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;*/

    m_pAACAudioDecoder->Reset();

    // m_pAACAudioDecoder->SetAudioParams(&m_vPar);

    return MFX_ERR_NONE;
}

mfxStatus AudioDECODEAAC::Close(void)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit || !m_pAACAudioDecoder.get())
        return MFX_ERR_NOT_INITIALIZED;

    mOutData.Close();
    mInData.Close();
    m_pAACAudioDecoder->Close();

    m_isInit = false;

    if (m_frame.Data)
    {
        delete[] m_frame.Data;
        m_frame.Data = NULL;
    }
    return MFX_ERR_NONE;
}



mfxStatus AudioDECODEAAC::Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out)
{
    //    mfxStatus  MFXSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(out);

    return MFX_AAC_Utility::Query(core, in, out);
}

mfxStatus AudioDECODEAAC::GetAudioParam(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    mfxStatus  MFXSts = MFX_ERR_NONE;
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    MFX_INTERNAL_CPY(&par->mfx, &m_vPar.mfx, sizeof(mfxInfoMFX));

    UMC::AACDecoderParams params;
    UMC::Status sts = m_pAACAudioDecoder->GetInfo(&params);
    MFX_CHECK_UMC_STS(sts);

    MFXSts = MFX_AAC_Utility::FillAudioParamByUMC(&params, &m_vPar);
    if (MFXSts != MFX_ERR_NONE)
        return MFXSts;

    MFXSts = MFX_AAC_Utility::FillAudioParam(&m_vPar, par);
    if (MFXSts != MFX_ERR_NONE)
        return MFXSts;

    par->Protected = m_vPar.Protected;


    return MFX_ERR_NONE;
}

mfxStatus AudioDECODEAAC::DecodeHeader(AudioCORE *core, mfxBitstream *bs, mfxAudioParam *par)
{
    core;
    MFX_CHECK_NULL_PTR2(bs, par);

    mfxStatus sts = CheckBitstream(bs);
    if (sts != MFX_ERR_NONE)
        return sts;
    if(par->mfx.CodecId == MFX_CODEC_AAC)
    {
        sAudio_specific_config audio_config_data; 
        sAdif_header audio_adif_data;
        sAdts_fixed_header audio_adts_fixed_data;
        sAdts_variable_header audio_adts_variable_data;
        sBitsreamBuffer BS;
        GET_INIT_BITSTREAM(&BS, bs->Data)
            BS.nDataLen = (Ipp32s)bs->DataLength;
        bs_save(&BS);
        if(BS.nDataLen != 0)
        {
            if (UMC::DecodeADIFSHeader(&audio_adif_data, &BS) == UMC::UMC_OK)
            {
                int copyHeaderSize = bs->DataLength < MAXIMUM_HEADER_LENGTH ? bs->DataLength : MAXIMUM_HEADER_LENGTH;
                ippsCopy_8u(bs->Data + bs->DataOffset, par->mfx.AACHeaderData, copyHeaderSize);
                par->mfx.AACHeaderDataSize = (mfxU16)copyHeaderSize;
                return FillAudioParamADIF(&audio_adif_data, par);
            }
            else  
            {
                bs_restore(&BS);
                if (UMC::DecodeADTSfixedHeader(&audio_adts_fixed_data, &BS) == UMC::UMC_OK 
                    && UMC::DecodeADTSvariableHeader(&audio_adts_variable_data, &BS) == UMC::UMC_OK)
                {
                    int copyHeaderSize = bs->DataLength < MAXIMUM_HEADER_LENGTH ? bs->DataLength : MAXIMUM_HEADER_LENGTH;
                    ippsCopy_8u(bs->Data + bs->DataOffset, par->mfx.AACHeaderData, copyHeaderSize);
                    par->mfx.AACHeaderDataSize = (mfxU16)copyHeaderSize;
                    return FillAudioParamADTSFixed(&audio_adts_fixed_data, par);
                }
                else
                {
                    bs_restore(&BS);
                    if(UMC::DecodeESDSHeader(&audio_config_data, &BS) == UMC::UMC_OK)
                    {
                        int copyHeaderSize = bs->DataLength < MAXIMUM_HEADER_LENGTH ?  bs->DataLength : MAXIMUM_HEADER_LENGTH;

                        ippsCopy_8u(bs->Data + bs->DataOffset, par->mfx.AACHeaderData, copyHeaderSize);
                        par->mfx.AACHeaderDataSize = (mfxU16)copyHeaderSize;

                        if (MFX_ERR_NONE != sts)
                        {
                            return sts;
                        }  
                        return FillAudioParamESDS(&audio_config_data, par);
                    }
                    else
                    {
                        return MFX_ERR_UNSUPPORTED;
                    } //end if(UMC::DecodeESDSHeader(&audio_config_data, &BS) == UMC::UMC_OK)
                } //end if(UMC::DecodeADTSfixedHeader(&audio_adts_fixed_data, &BS) == UMC::UMC_OK)
            } // end if (UMC::DecodeADIFSHeader(&audio_adif_data, &BS) == UMC::UMC_OK)
        }
        else 
        {
            return MFX_ERR_NONE;
        } //end  if(BS.nDataLen != 0)
    }// end if(par->mfx.CodecId == MFX_CODEC_AAC)
    return MFX_ERR_UNSUPPORTED;
}


//[SS] to do implement this function
mfxStatus AudioDECODEAAC::FillAudioParamESDS(sAudio_specific_config* config, mfxAudioParam *out)
{
    mfxStatus sts = MFX_ERR_NONE;
    out->mfx.SampleFrequency = (mfxU32)config->samplingFrequency;


    out->mfx.CodecId = MFX_CODEC_AAC;
    out->mfx.CodecProfile = (mfxU16)config->audioObjectType;

    if (config->psPresentFlag == -1)
    {
        out->mfx.FlagPSSupportLev = MFX_AUDIO_AAC_PS_DISABLE;
    }
    else
    {
        out->mfx.FlagPSSupportLev = MFX_AUDIO_AAC_PS_PARSER;
    }

    return sts;
}

mfxStatus AudioDECODEAAC::FillAudioParamADIF(sAdif_header* config, mfxAudioParam *out)
{
    mfxStatus sts = MFX_ERR_NONE;
    sProgram_config_element *m_p_pce = &config->pce[0];
    if (m_p_pce == NULL)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    if ((m_p_pce->sampling_frequency_index < 0) || (m_p_pce->sampling_frequency_index > 12))
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    out->mfx.SampleFrequency = (mfxU32)aac_sampling_frequency_table[m_p_pce->sampling_frequency_index];;

    out->mfx.CodecId = MFX_CODEC_AAC;

    switch (m_p_pce->object_type) {
case 0:
    out->mfx.CodecProfile = AOT_AAC_MAIN;
    break;
case 1:
    out->mfx.CodecProfile = AOT_AAC_LC;
    break;
case 2:
    out->mfx.CodecProfile = AOT_AAC_SSR;
    break;
case 3:
    out->mfx.CodecProfile = AOT_AAC_LTP;
    break;
    }

    out->mfx.FlagPSSupportLev = MFX_AUDIO_AAC_PS_PARSER;

    return sts;
}

mfxStatus AudioDECODEAAC::FillAudioParamADTSFixed(sAdts_fixed_header* config, mfxAudioParam *out)
{
    mfxStatus sts = MFX_ERR_NONE;
    if ((config->sampling_frequency_index < 0) || (config->sampling_frequency_index > 12))
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    out->mfx.SampleFrequency = (mfxU32)aac_sampling_frequency_table[config->sampling_frequency_index];;
    out->mfx.NumChannel = (mfxU16)config->channel_configuration;

    out->mfx.CodecId = MFX_CODEC_AAC;
    out->mfx.CodecProfile = (mfxU16)get_audio_object_type_by_adts_header(config);;

    out->mfx.FlagPSSupportLev = MFX_AUDIO_AAC_PS_PARSER;

    return sts;
}


mfxStatus AudioDECODEAAC::QueryIOSize(AudioCORE *core, mfxAudioParam *par, mfxAudioAllocRequest *request)
{
    core;
    MFX_CHECK_NULL_PTR2(par, request);

    request->SuggestedInputSize = CH_MAX * 768;;
    request->SuggestedOutputSize = CH_MAX * 1024 * 2 /* up sampling SBR */ * sizeof(Ipp16s);

    return MFX_ERR_NONE;
}


mfxStatus AudioDECODEAAC::DecodeFrameCheck(mfxBitstream *bs,
                                           mfxAudioFrame *aFrame,
                                           MFX_ENTRY_POINT *pEntryPoint)
{
    mfxStatus mfxSts = DecodeFrameCheck(bs, aFrame);

    if (MFX_ERR_NONE == mfxSts) // It can be useful to run threads right after first frame receive
    {
        ThreadAudioDecodeTaskInfo * info = new ThreadAudioDecodeTaskInfo();
        info->out = aFrame;

        pEntryPoint->pRoutine = &AACDECODERoutine;
        pEntryPoint->pCompleteProc = &AACCompleteProc;
        pEntryPoint->pState = this;
        pEntryPoint->requiredNumThreads = 1; // need only one thread
        pEntryPoint->pParam = info;

        //TODO: disable WA for synchronous decoding of first frame
        if (0 == m_nUncompFrameSize) {
            mfxSts = AudioDECODEAAC::AACDECODERoutine(this, info, 0, 0);
            m_nUncompFrameSize = aFrame->DataLength;
        }
    }

    return mfxSts;
}

mfxStatus AudioDECODEAAC::AACDECODERoutine(void *pState, void *pParam,
                                          mfxU32 threadNumber, mfxU32 callNumber)
{
    callNumber;
    threadNumber;
    AudioDECODEAAC &obj = *((AudioDECODEAAC *) pState);
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX::AutoTimer timer("DecodeFrame");

    if (MFX_PLATFORM_SOFTWARE == obj.m_platform)
    {
        ThreadAudioDecodeTaskInfo *pTask = (ThreadAudioDecodeTaskInfo *) pParam;

        obj.mInData.SetBufferPointer((Ipp8u *)obj.m_frame.Data + obj.m_frame.DataOffset, obj.m_frame.DataLength);
        obj.mInData.SetDataSize(obj.m_frame.DataLength);

        obj.mOutData.SetBufferPointer( static_cast<Ipp8u *>(pTask->out->Data), pTask->out->DataLength ? pTask->out->DataLength : pTask->out->MaxLength);
        if (0 != obj.mInData.GetDataSize()) {
            UMC::Status sts = obj.m_pAACAudioDecoder.get()->GetFrame(&obj.mInData, &obj.mOutData);
            mfxRes = ConvertStatusUmc2Mfx(sts);
            //TODO: disable WA for decoding first frame
            if (!pTask->out->DataLength) {
                pTask->out->DataLength = (mfxU32) obj.mOutData.GetDataSize();
            }
        }

        // set data size 0 to the input buffer 
        // set out buffer size;
        memmove(obj.m_frame.Data + obj.m_frame.DataOffset, obj.mInData.GetDataPointer(), obj.mInData.GetDataSize());
        obj.m_frame.DataLength = (mfxU32) obj.mInData.GetDataSize();
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return mfxRes;

} // mfxStatus AudioDECODEAAC::AACECODERoutine(void *pState, void *pParam,

mfxStatus AudioDECODEAAC::AACCompleteProc(void *pState, void *pParam,
                                          mfxStatus taskRes)
{
    taskRes;
    pParam;
    AudioDECODEAAC &obj = *((AudioDECODEAAC *) pState);

    if (MFX_PLATFORM_SOFTWARE == obj.m_platform)
    {
        ThreadAudioDecodeTaskInfo *pTask = (ThreadAudioDecodeTaskInfo *) pParam;
        if (pTask)
            delete pTask;
        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }
}

mfxStatus AudioDECODEAAC::DecodeFrameCheck(mfxBitstream *bs, mfxAudioFrame *aFrame)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    aFrame;
    mfxStatus sts;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
    if (sts != MFX_ERR_NONE)
        return sts;

    //    UMC::Status umcRes = UMC::UMC_OK;
    if (NULL != bs)
    {
        sts = CheckBitstream(bs);
        MFX_CHECK_STS(sts);

        sts = ConstructFrame(bs, &m_frame, &aFrame->DataLength);

        if (MFX_ERR_NONE != sts)
        {
            return sts;
        }
        if (bs) {
            aFrame->TimeStamp = bs->TimeStamp;
        }
    }

    return sts;
}


mfxStatus AudioDECODEAAC::DecodeFrame(mfxBitstream *bs, mfxAudioFrame *aFrame)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    aFrame;
    mfxStatus sts;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;
    if(bs)
    {
        sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
        if (sts != MFX_ERR_NONE)
            return sts;
    }
    else
    {
        return MFX_ERR_MORE_DATA;
    }
    //    UMC::Status umcRes = UMC::UMC_OK;

    if (NULL != bs)
    {
        sts = ConstructFrame(bs, &m_frame, &aFrame->DataLength);

        if (MFX_ERR_NONE != sts)
        {
            return sts;
        }
    }

    return sts;
}

// protected methods
mfxStatus AudioDECODEAAC::CopyBitstream(mfxBitstream& bs, const mfxU8* ptr, mfxU32 bytes)
{
    if (bs.DataOffset + bs.DataLength + bytes > bs.MaxLength)
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    ippsCopy_8u(ptr, bs.Data + bs.DataOffset + bs.DataLength, bytes);
    bs.DataLength += bytes;
    return MFX_ERR_NONE;
}

void AudioDECODEAAC::MoveBitstreamData(mfxBitstream& bs, mfxU32 offset)
{
    VM_ASSERT(offset <= bs.DataLength);
    bs.DataOffset += offset;
    bs.DataLength -= offset;
}

mfxStatus AudioDECODEAAC::ConstructFrame(mfxBitstream *in, mfxBitstream *out, unsigned int *pUncompFrameSize)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR2(in, out);

    MFX_CHECK_NULL_PTR2(in->Data, out->Data);
    Ipp32s inBufferSize;

    mInData.SetBufferPointer((Ipp8u *)in->Data + in->DataOffset, in->DataLength);
    mInData.SetDataSize(in->DataLength);

    UMC::Status stsUMC = m_pAACAudioDecoder->FrameConstruct(&mInData, &inBufferSize);
    switch (stsUMC)
    {
    case UMC::UMC_OK: 
        
        if (in->DataFlag == MFX_BITSTREAM_COMPLETE_FRAME)
        {
            sts = CopyBitstream(*out, in->Data + in->DataOffset, in->DataLength);
            if(sts != MFX_ERR_NONE) 
            {
                return MFX_ERR_NOT_ENOUGH_BUFFER;
            }
            MoveBitstreamData(*in, (mfxU32)(in->DataLength));
        }
        else
        {
            if(m_inputFormat == MFX_AUDIO_AAC_ADTS)
            {
                sts = CopyBitstream(*out, in->Data + in->DataOffset, inBufferSize);
                if(sts != MFX_ERR_NONE) 
                {
                    return MFX_ERR_NOT_ENOUGH_BUFFER;
                }
                MoveBitstreamData(*in, (mfxU32)(inBufferSize));
            } else if (m_inputFormat == MFX_AUDIO_AAC_ESDS || m_inputFormat == MFX_AUDIO_AAC_ADIF)  // AR need to check logic for ADIF headers
            {
                sts = CopyBitstream(*out, in->Data + in->DataOffset, in->DataLength);
                if(sts != MFX_ERR_NONE) 
                {
                    return MFX_ERR_NOT_ENOUGH_BUFFER;
                }
                MoveBitstreamData(*in, (mfxU32)(in->DataLength));
            } else 
            {
                return MFX_ERR_UNSUPPORTED;
            }

        }
        
        *pUncompFrameSize = 1024 * sizeof(Ipp16s) * 2;
        
        //TODO: find number of channels as well
        *pUncompFrameSize = m_nUncompFrameSize;
        break;
    case UMC::UMC_ERR_NOT_ENOUGH_DATA:
        return MFX_ERR_MORE_DATA;
        break;
    default:
        return MFX_ERR_NONE;
        break;
    }


    /* AppendBitstream(*out, in->Data + in->DataOffset, in->DataLength);
    MoveBitstreamData(*in, (mfxU32)(in->DataLength)); */

    return MFX_ERR_NONE;

}

mfxStatus MFX_AAC_Utility::FillAudioParam( mfxAudioParam *in, mfxAudioParam *out)
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

mfxStatus MFX_AAC_Utility::FillAudioParamByUMC(UMC::AACDecoderParams *in, mfxAudioParam *out)
{
    out->mfx.BitPerSample = (mfxU16)in->m_info.bitPerSample;
    out->mfx.Bitrate = (mfxU32)in->m_info.bitrate;
    out->mfx.NumChannel = (mfxU16)in->m_info.channels;
    out->mfx.SampleFrequency = (mfxU32)in->m_info.sample_frequency;
    return MFX_ERR_NONE;
}

mfxStatus MFX_AAC_Utility::Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (in == out)
    {
        mfxAudioParam in1;
        MFX_INTERNAL_CPY(&in1, in, sizeof(mfxAudioParam));
        return Query(core, &in1, out);
    }

    memset(&out->mfx, 0, sizeof(mfxAudioInfoMFX));

    if (in)
    {
        if (in->mfx.CodecId == MFX_CODEC_AAC)
            out->mfx.CodecId = in->mfx.CodecId;

        //to do add checks
        out->mfx.Bitrate = in->mfx.Bitrate;
        out->mfx.SampleFrequency = in->mfx.SampleFrequency;
        out->mfx.NumChannel = in->mfx.NumChannel;
        out->mfx.BitPerSample = in->mfx.BitPerSample;
        switch(in->mfx.CodecProfile)
        {
        case MFX_PROFILE_AAC_LC :
        case MFX_PROFILE_AAC_LTP:
        case MFX_PROFILE_AAC_MAIN :
        case MFX_PROFILE_AAC_SSR :
        case MFX_PROFILE_AAC_HE :
        case MFX_PROFILE_AAC_ALS :
        case MFX_PROFILE_AAC_BSAC :
        case MFX_PROFILE_AAC_PS :
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

        mfxU32 flagPSSupportLev = in->mfx.FlagPSSupportLev;
        if (flagPSSupportLev == MFX_AUDIO_AAC_PS_DISABLE || 
            flagPSSupportLev == MFX_AUDIO_AAC_PS_PARSER ||
            flagPSSupportLev == MFX_AUDIO_AAC_PS_ENABLE_BL ||
            flagPSSupportLev == MFX_AUDIO_AAC_PS_ENABLE_UR 
            )
        {
            out->mfx.FlagPSSupportLev = in->mfx.FlagPSSupportLev;
        }
        else
        {
            if(flagPSSupportLev == 0)
            {
                out->mfx.FlagPSSupportLev = MFX_AUDIO_AAC_PS_ENABLE_UR;
            }
            else
            {
                sts = MFX_ERR_UNSUPPORTED;
            }
        }
        if(in->mfx.AACHeaderDataSize)
        {
            MFX_INTERNAL_CPY(out->mfx.AACHeaderData, in->mfx.AACHeaderData,in->mfx.AACHeaderDataSize);
            out->mfx.AACHeaderDataSize = in->mfx.AACHeaderDataSize;
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



#endif // MFX_ENABLE_AAC_AUDIO_DECODE
