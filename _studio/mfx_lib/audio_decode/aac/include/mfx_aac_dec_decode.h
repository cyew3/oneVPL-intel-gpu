//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_AAC_AUDIO_DECODE)

#include "umc_defs.h"

#ifndef _MFX_AAC_DEC_DECODE_H_
#define _MFX_AAC_DEC_DECODE_H_

#include "mfxaudio.h"
#include "mfxaudio++int.h"
#include "umc_audio_decoder.h"
#include "umc_aac_decoder.h"
#include "umc_mutex.h"

#include "ippcore.h"
#include "ipps.h"
#include "mfx_task.h"
#include "mfxpcp.h"

#include <queue>
#include <list>
#include <memory>

namespace UMC
{
    class AudioData;
    class AACDecoder;
    class AACDecoderParams; 
} // namespace UMC

enum
{
    MINIMAL_BITSTREAM_LENGTH    = 4,
    MAXIMUM_HEADER_LENGTH       = 64,
};

enum 
{
   MFX_AUDIO_AAC_ESDS = 4,
};

class AudioDECODE;
class AudioDECODEAAC : public AudioDECODE
{
public:
    static mfxStatus Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus QueryIOSize(AudioCORE *core, mfxAudioParam *par, mfxAudioAllocRequest *request);
    static mfxStatus DecodeHeader(AudioCORE *core, mfxBitstream *bs, mfxAudioParam* par);

    AudioDECODEAAC(AudioCORE *core, mfxStatus * sts);
    virtual ~AudioDECODEAAC(void);

    mfxStatus Init(mfxAudioParam *par);
    virtual mfxStatus Reset(mfxAudioParam *par);
    virtual mfxStatus Close(void);
  
    virtual mfxStatus GetAudioParam(mfxAudioParam *par);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxAudioFrame *buffer_out);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxAudioFrame *buffer_out, MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxAudioFrame *buffer_out);

protected:
     virtual mfxStatus ConstructFrame(mfxBitstream *in, mfxBitstream *out, unsigned int *pUncompFrameSize);

     static mfxStatus FillAudioParamESDS(sAudio_specific_config* config, mfxAudioParam *out);
     static mfxStatus FillAudioParamADIF(sAdif_header* config, mfxAudioParam *out);
     static mfxStatus FillAudioParamADTSFixed(sAdts_fixed_header* config, mfxAudioParam *out);
     

     static mfxStatus AACDECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
     static mfxStatus AACCompleteProc(void *pState, void *pParam, mfxStatus taskRes);

     mfxStatus CopyBitstream(mfxBitstream& bs, const mfxU8* ptr, mfxU32 bytes);
     void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset);
    // UMC decoder 
    std::auto_ptr<UMC::AACDecoder>  m_pAACAudioDecoder;
    UMC::MediaData        mInData;
    UMC::MediaData        mOutData;
    // end UMC decoder

    mfxBitstream m_frame;
    mfxU16 m_inputFormat;

    mfxAudioParam m_vPar; // internal params storage

    AudioCORE * m_core;

    eMFXPlatform m_platform;
    bool    m_isInit;

    UMC::Mutex m_mGuard;
    mfxU32 m_nUncompFrameSize;
};

#endif // _MFX_AAC_DEC_DECODE_H_
#endif
