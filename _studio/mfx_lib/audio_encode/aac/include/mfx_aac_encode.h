//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_AAC_AUDIO_ENCODE)

#include "umc_defs.h"

#ifndef __MFX_AAC_ENCODE_H__
#define __MFX_AAC_ENCODE_H__


#include "mfxaudio.h"
#include "mfxaudio++int.h"
#include "umc_audio_decoder.h"
#include "umc_aac_encoder.h"
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
    class AACEncoder;
    class AACEncoderParams; 
} // namespace UMC


class AudioENCODEAAC : public AudioENCODE {
public:

    static mfxStatus Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus QueryIOSize(AudioCORE *core, mfxAudioParam *par, mfxAudioAllocRequest *request);

    AudioENCODEAAC(AudioCORE *core, mfxStatus *status);
    virtual ~AudioENCODEAAC();
    virtual mfxStatus Init(mfxAudioParam *par);

    virtual mfxStatus Reset(mfxAudioParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetAudioParam(mfxAudioParam *par);

    virtual mfxStatus EncodeFrame(mfxBitstream *bs, mfxBitstream *buffer_out);
    virtual mfxStatus EncodeFrameCheck(mfxBitstream *bs, mfxBitstream *buffer_out);
    virtual mfxStatus EncodeFrameCheck(mfxBitstream *bs, mfxBitstream *buffer_out,
        MFX_ENTRY_POINT *pEntryPoint);
 

protected:

    static mfxStatus AACECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus AACAbortProc(void *pState, void *pParam);
    static mfxStatus AACCompleteProc(void *pState, void *pParam, mfxStatus taskRes);

    mfxStatus CopyBitstream(mfxBitstream& bs, const mfxU8* ptr, mfxU32 bytes);
    void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset);

    mfxStatus AudioENCODEAAC::ConstructFrame(mfxBitstream *in, mfxBitstream *out);

   // UMC encoder 
    std::auto_ptr<UMC::AACEncoder>  m_pAACAudioEncoder;
    UMC::MediaData        mInData;
    UMC::MediaData        mOutData;
    // end UMC encoder

    mfxBitstream m_frame;
    mfxU16 m_inputFormat;

    mfxAudioParam m_vPar; // internal params storage

    AudioCORE * m_core;

    eMFXPlatform m_platform;
    bool    m_isInit;

    UMC::Mutex m_mGuard;





};

#endif // __MFX_AAC_ENCODE_H__

#endif // MFX_ENABLE_AAC_AUDIO_ENCODE
