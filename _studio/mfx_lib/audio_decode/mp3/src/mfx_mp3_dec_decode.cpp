//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_mp3_dec_decode.h"
#include "mfx_common.h"
#if defined (MFX_ENABLE_MP3_AUDIO_DECODE)

#include "mfx_common_int.h"
#include "mfx_thread_task.h"
#include "umc_defs.h"


class MFX_MP3_Utility
{
public:
    static mfxStatus Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus FillAudioParam( mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus FillAudioParamByUMC(UMC::MP3DecoderParams *in, mfxAudioParam *out);
};

AudioDECODEMP3::AudioDECODEMP3(AudioCORE *core, mfxStatus * sts)
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

AudioDECODEMP3::~AudioDECODEMP3(void)
{
    Close();
}

mfxStatus AudioDECODEMP3::Init(mfxAudioParam *par)
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
        m_pMP3AudioDecoder.reset(new UMC::MP3Decoder());
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }
    // allocate memory
    //
    Ipp32u MaxLength = MAX_MP3_INPUT_DATA_SIZE; 

    memset(&m_frame, 0, sizeof(mfxBitstream));
    m_frame.MaxLength = MaxLength;
    m_frame.Data = new mfxU8[MaxLength];
    m_frame.DataLength = 0;
    m_frame.DataOffset = 0;

    UMC::MP3DecoderParams params;
    params.mc_lfe_filter_off = 1;

    params.synchro_mode = 0;

    mInData.SetDataSize(0);
    mOutData.SetDataSize(0);
    params.m_pData = NULL;
    params.lpMemoryAllocator = NULL;
   // params.m_info.stream_type = UMC::AAC_AUDIO;

    UMC::Status sts = m_pMP3AudioDecoder->Init(&params);
    MFX_CHECK_UMC_STS(sts);
    delete params.m_pData;
    params.m_pData = NULL;
    m_isInit = true;

    return MFX_ERR_NONE;
}

mfxStatus AudioDECODEMP3::Reset(mfxAudioParam *par)
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

    m_pMP3AudioDecoder->Reset();

    // m_pMP3AudioDecoder->SetAudioParams(&m_vPar);

    return MFX_ERR_NONE;
}

mfxStatus AudioDECODEMP3::Close(void)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit || !m_pMP3AudioDecoder.get())
        return MFX_ERR_NOT_INITIALIZED;

    mOutData.Close();
    mInData.Close();
    m_pMP3AudioDecoder->Close();

    if (m_frame.Data)
    {
        delete[] m_frame.Data;
        m_frame.Data = NULL;
    }
    m_isInit = false;
    return MFX_ERR_NONE;
}

mfxStatus AudioDECODEMP3::Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out)
{
//    mfxStatus  MFXSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(out);

    return MFX_MP3_Utility::Query(core, in, out);
}

mfxStatus AudioDECODEMP3::GetAudioParam(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    mfxStatus  MFXSts = MFX_ERR_NONE;
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    MFX_INTERNAL_CPY(&par->mfx, &m_vPar.mfx, sizeof(mfxAudioInfoMFX));

    UMC::MP3DecoderParams params;
    UMC::Status sts = m_pMP3AudioDecoder->GetInfo(&params);
    MFX_CHECK_UMC_STS(sts);

    MFXSts = MFX_MP3_Utility::FillAudioParamByUMC(&params, &m_vPar);
    if (MFXSts != MFX_ERR_NONE)
        return MFXSts;
    
    MFXSts = MFX_MP3_Utility::FillAudioParam(&m_vPar, par);
    if (MFXSts != MFX_ERR_NONE)
        return MFXSts;

    par->Protected = m_vPar.Protected;


    return MFX_ERR_NONE;
}

mfxStatus AudioDECODEMP3::DecodeHeader(AudioCORE *core, mfxBitstream *bs, mfxAudioParam *par)
{
    core;
    MFX_CHECK_NULL_PTR2(bs, par);

    mfxStatus sts = CheckBitstream(bs);
    if (sts != MFX_ERR_NONE)
        return sts;
    if(par->mfx.CodecId == MFX_CODEC_MP3)
    {
        MP3Dec_com res;
        memset((void*)&res, 0, sizeof(MP3Dec_com));
        if(bs->DataLength != 0)
        {
            UMC::Status sts =  UMC::DecodeMP3Header(bs->Data,bs->DataLength, &res);
            MFX_CHECK_UMC_STS(sts);
            return FillAudioParamMP3(&res, par);
        }
        else 
        {
            return MFX_ERR_NONE;
        } 
    }
    return MFX_ERR_NONE;
}

mfxStatus AudioDECODEMP3::FillAudioParamMP3(MP3Dec_com* res, mfxAudioParam *out)
{
    mfxStatus sts = MFX_ERR_NONE;
    out->mfx.BitPerSample = 16; // UMC MP3 decoder retunrs 16 bit per sample depth (see mp3dec_api_fp.cpp line 405)
    out->mfx.NumChannel = (mfxU16)(res->stereo); // to do check if it works correct on mono
    out->mfx.Bitrate = (mfxU32)mp3_bitrate[res->header.id][res->header.layer - 1][res->header.bitRate];
    out->mfx.SampleFrequency = (mfxU32)(mp3_frequency[res->header.id + res->mpg25][res->header.samplingFreq] + res->header.paddingBit);

    out->mfx.CodecId = MFX_CODEC_MP3;
    if(res->header.id == 1)
    {
        switch (res->header.layer) {
        case 1:
            out->mfx.CodecLevel = MFX_AUDIO_MPEG1_LAYER1;
            break;
        case 2:
            out->mfx.CodecLevel = MFX_AUDIO_MPEG1_LAYER2;
            break;
        case 3:
            out->mfx.CodecLevel = MFX_AUDIO_MPEG1_LAYER3;
            break;
        }
    }
    else
    {
        if(res->header.id == 2)
        {
            switch (res->header.layer) {
        case 1:
            out->mfx.CodecLevel = MFX_AUDIO_MPEG2_LAYER1;
            break;
        case 2:
            out->mfx.CodecLevel = MFX_AUDIO_MPEG2_LAYER2;
            break;
        case 3:
            out->mfx.CodecLevel = MFX_AUDIO_MPEG2_LAYER3;
            break;
            }
        }
    }
    return sts;
}

mfxStatus AudioDECODEMP3::QueryIOSize(AudioCORE *core, mfxAudioParam *par, mfxAudioAllocRequest *request)
{
    core;
    MFX_CHECK_NULL_PTR2(par, request);

    request->SuggestedInputSize = MAX_MP3_INPUT_DATA_SIZE;
    request->SuggestedOutputSize = 1152 * (NUM_CHANNELS + 1) * sizeof(Ipp16s);

    return MFX_ERR_NONE;
}


mfxStatus AudioDECODEMP3::DecodeFrameCheck(mfxBitstream *bs,
                                           mfxAudioFrame *aFrame,
                                           MFX_ENTRY_POINT *pEntryPoint)
{
    mfxStatus mfxSts = DecodeFrameCheck(bs, aFrame);

    if (MFX_ERR_NONE == mfxSts) // It can be useful to run threads right after first frame receive
    {
        ThreadAudioDecodeTaskInfo * info = new ThreadAudioDecodeTaskInfo();
        info->out = aFrame;

        pEntryPoint->pRoutine = &MP3ECODERoutine;
        pEntryPoint->pCompleteProc = &MP3CompleteProc;
        pEntryPoint->pState = this;
        pEntryPoint->requiredNumThreads = 1; // need only one thread
        pEntryPoint->pParam = info;
        //TODO: disable WA for synchronous decoding of first frame
        if (0 == m_nUncompFrameSize) {
            mfxSts = AudioDECODEMP3::MP3ECODERoutine(this, info, 0, 0);
            m_nUncompFrameSize = aFrame->DataLength;
        }
    }


    return mfxSts;
}

mfxStatus AudioDECODEMP3::MP3ECODERoutine(void *pState, void *pParam,
                                          mfxU32 threadNumber, mfxU32 callNumber)
{
    callNumber;
    threadNumber;
    AudioDECODEMP3 &obj = *((AudioDECODEMP3 *) pState);
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX::AutoTimer timer("DecodeFrame");

    if (MFX_PLATFORM_SOFTWARE == obj.m_platform)
    {
        ThreadAudioDecodeTaskInfo *pTask = (ThreadAudioDecodeTaskInfo *) pParam;

        obj.mInData.SetBufferPointer((Ipp8u *)obj.m_frame.Data + obj.m_frame.DataOffset, obj.m_frame.DataLength);
        obj.mInData.SetDataSize(obj.m_frame.DataLength);

        //datalen computed in syncpart should equal to decoded datalen
        obj.mOutData.SetBufferPointer( static_cast<Ipp8u *>(pTask->out->Data), pTask->out->MaxLength);
        //obj.mInData.GetDataSize() value may be equivalent only encoded frame size, or, in case free format stream, 3 bytes
        //because in this case we must save encoded frame size + 3 bytes (syncword - 1) 
        if (0 != obj.mInData.GetDataSize() && obj.mInData.GetDataSize() != 3) {
            UMC::Status sts = obj.m_pMP3AudioDecoder.get()->GetFrame(&obj.mInData, &obj.mOutData);
            mfxRes = ConvertStatusUmc2Mfx(sts);
            //TODO: disable WA for decoding first frame
            pTask->out->DataLength = (mfxU32) obj.mOutData.GetDataSize();
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

} // mfxStatus AudioDECODEMP3::MP3ECODERoutine(void *pState, void *pParam,

mfxStatus AudioDECODEMP3::MP3CompleteProc(void *pState, void *pParam,
                                          mfxStatus taskRes)
{
    taskRes;
    pParam;
    AudioDECODEMP3 &obj = *((AudioDECODEMP3 *) pState);

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

mfxStatus AudioDECODEMP3::DecodeFrameCheck(mfxBitstream *bs, mfxAudioFrame *aFrame)
{
    aFrame;
    UMC::AutomaticUMCMutex guard(m_mGuard);
    mfxStatus sts;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
    if (sts != MFX_ERR_NONE)
        return sts;

//    UMC::Status umcRes = UMC::UMC_OK;
    if (NULL != bs) {
        sts = CheckBitstream(bs);
        MFX_CHECK_STS(sts);

        unsigned int RawFrameSize;
        sts = ConstructFrame(bs, &m_frame, &RawFrameSize);
        MFX_CHECK_STS(sts);

        //check that buffer_out.MaxLength < RawFrameSize
        if (aFrame->MaxLength < RawFrameSize) {
            sts = MFX_ERR_NOT_ENOUGH_BUFFER;
        } else {

            if (bs) {
                aFrame->TimeStamp = bs->TimeStamp;
            }
            aFrame->DataLength = RawFrameSize;;
        }
    }
    return sts;
}

mfxStatus AudioDECODEMP3::DecodeFrame(mfxBitstream *bs, mfxAudioFrame *aFrame)
{
    aFrame;
    UMC::AutomaticUMCMutex guard(m_mGuard);
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
        unsigned int RawFrameSize;
        sts = ConstructFrame(bs, &m_frame,  &RawFrameSize);

        if (MFX_ERR_NONE != sts)
        {
            return sts;
        }
    }

    return sts;
}

// protected methods
mfxStatus AudioDECODEMP3::CopyBitstream(mfxBitstream& bs, const mfxU8* ptr, mfxU32 bytes)
{
    if (bs.DataOffset + bs.DataLength + bytes > bs.MaxLength)
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    ippsCopy_8u(ptr, bs.Data, bytes);
    bs.DataLength = bytes;
    bs.DataOffset = 0;
    return MFX_ERR_NONE;
}

void AudioDECODEMP3::MoveBitstreamData(mfxBitstream& bs, mfxU32 offset)
{
    VM_ASSERT(offset <= bs.DataLength);
    bs.DataOffset += offset;
    bs.DataLength -= offset;
}

mfxStatus AudioDECODEMP3::ConstructFrame(mfxBitstream *in, mfxBitstream *out, unsigned int *p_RawFrameSize)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR2(in, out);

    MFX_CHECK_NULL_PTR2(in->Data, out->Data);
    Ipp32s inBufferSize;
    Ipp32s inBufferID3HeaderSize;
    int bitRate = 0;

    mInData.SetBufferPointer((Ipp8u *)in->Data + in->DataOffset, in->DataLength);
    mInData.SetDataSize(in->DataLength);

    UMC::Status stsUMC = m_pMP3AudioDecoder->FrameConstruct(&mInData, &inBufferSize, &bitRate, &inBufferID3HeaderSize, p_RawFrameSize);
    switch (stsUMC)
    {
    case UMC::UMC_OK: 
        if (inBufferSize <= (Ipp32s)in->DataLength)
        {
            if (bitRate != 0)
                sts = CopyBitstream(*out, in->Data + in->DataOffset + inBufferID3HeaderSize, inBufferSize - inBufferID3HeaderSize);
            else //if stream has free format
            //check of case (inBufferSize - inBufferID3HeaderSize + 3) > in->DataLength is not necessary
            //because we can't decode last frame in free format stream (due umc mp3dec_GetSynch function design)
                sts = CopyBitstream(*out, in->Data + in->DataOffset + inBufferID3HeaderSize, inBufferSize - inBufferID3HeaderSize + 3);
                     
            if(sts != MFX_ERR_NONE) 
            {
                return MFX_ERR_NOT_ENOUGH_BUFFER;
            }

            MoveBitstreamData(*in, (mfxU32)(inBufferSize));
            *p_RawFrameSize = m_nUncompFrameSize;
        }
        else
        {
             return MFX_ERR_MORE_DATA;
        }
        break;
    case UMC::UMC_ERR_NOT_ENOUGH_DATA:
    case UMC::UMC_ERR_SYNC:
        MoveBitstreamData(*in, (mfxU32)(inBufferSize));
        return MFX_ERR_MORE_DATA;
    default:
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;

}

mfxStatus MFX_MP3_Utility::FillAudioParam( mfxAudioParam *in, mfxAudioParam *out)
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
    out->mfx.Layer = in->mfx.Layer;

    return MFX_ERR_NONE;
}

mfxStatus MFX_MP3_Utility::FillAudioParamByUMC(UMC::MP3DecoderParams *in, mfxAudioParam *out)
{
    out->mfx.BitPerSample = (mfxU16)in->m_info.bitPerSample;
    out->mfx.Bitrate = (mfxU32)in->m_info.bitrate;
    out->mfx.NumChannel = (mfxU16)in->m_info.channels;
    out->mfx.SampleFrequency = (mfxU32)in->m_info.sample_frequency;
    return MFX_ERR_NONE;
}

mfxStatus MFX_MP3_Utility::Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out)
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
        if (in->mfx.CodecId == MFX_CODEC_MP3)
            out->mfx.CodecId = in->mfx.CodecId;

        //to do add checks
        out->mfx.Bitrate = in->mfx.Bitrate;
        out->mfx.SampleFrequency = in->mfx.SampleFrequency;
        out->mfx.NumChannel = in->mfx.NumChannel;
        out->mfx.BitPerSample = in->mfx.BitPerSample;
        switch(in->mfx.Layer)
        {
        case MFX_AUDIO_MPEG1_LAYER1 :
        case MFX_AUDIO_MPEG1_LAYER2 :
        case MFX_AUDIO_MPEG1_LAYER3 :
        case MFX_AUDIO_MPEG2_LAYER1 :
        case MFX_AUDIO_MPEG2_LAYER2 :
        case MFX_AUDIO_MPEG2_LAYER3 :
            out->mfx.Layer = in->mfx.Layer;
            break;
        default:
            out->mfx.Layer = MFX_PROFILE_UNKNOWN;
        };

        switch (in->mfx.CodecProfile)
        {
        case MFX_PROFILE_UNKNOWN:
            out->mfx.CodecProfile = in->mfx.CodecProfile;
            break;
        }

    }
    else
    {
        out->mfx.CodecId = MFX_CODEC_MP3;
        out->mfx.CodecLevel = 0;
        out->AsyncDepth = 1;
    }        

    return sts;
}



#endif // MFX_ENABLE_MP3_AUDIO_DECODE
