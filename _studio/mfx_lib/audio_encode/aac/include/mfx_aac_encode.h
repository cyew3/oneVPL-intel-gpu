//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2016 Intel Corporation. All Rights Reserved.
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

#include "libmfx_core.h"
#include <vector>
#include <list>
#include <memory>

namespace UMC
{
    class AudioData;
    class AACEncoder;
    class AACEncoderParams; 
} // namespace UMC

class CommonCORE;

class AudioENCODEAAC : public AudioENCODE {
public:

    static mfxStatus Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus QueryIOSize(AudioCORE *core, mfxAudioParam *par, mfxAudioAllocRequest *request);

    AudioENCODEAAC(CommonCORE *core, mfxStatus *status);
    virtual ~AudioENCODEAAC();
    virtual mfxStatus Init(mfxAudioParam *par);

    virtual mfxStatus Reset(mfxAudioParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetAudioParam(mfxAudioParam *par);

    virtual mfxStatus EncodeFrame(mfxAudioFrame *, mfxBitstream *) {
        return MFX_ERR_NONE;
    };
    virtual mfxStatus EncodeFrameCheck(mfxAudioFrame *bs, mfxBitstream *buffer_out);
    virtual mfxStatus EncodeFrameCheck(mfxAudioFrame *bs, mfxBitstream *buffer_out, MFX_ENTRY_POINT *pEntryPoint);
 

protected:

    static mfxStatus AACENCODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus AACCompleteProc(void *pState, void *pParam, mfxStatus taskRes);

    mfxStatus CopyBitstream(mfxAudioFrame& bs, const mfxU8* ptr, mfxU32 bytes);
    void MoveBitstreamData(mfxAudioFrame& bs, mfxU32 offset);

   // UMC encoder 
    std::auto_ptr<UMC::AACEncoder>  m_pAACAudioEncoder;
    UMC::MediaData        mInData;
    UMC::MediaData        mOutData;
    // end UMC encoder

    //mfxAudioFrame m_frame;
    mfxU16 m_inputFormat;

    mfxAudioParam m_vPar; // internal params storage

    AudioCORE * m_core;
    CommonCORE * m_CommonCore;

    eMFXPlatform m_platform;
    bool    m_isInit;
    mfxI32  m_dTimestampShift;

    UMC::Mutex m_mGuard;

    class AudioFramesCollector {
        UMC::Mutex mGuard;
        CommonCORE& mCore;
        std::list<mfxAudioFrame*> list;
        std::vector<mfxU8>& buffer;
        size_t offset;
    public:
        AudioFramesCollector(std::vector<mfxU8>& v, CommonCORE& core)
            : mCore(core)
            , buffer(v)
            , offset(0) {
        }
        void push(mfxAudioFrame* t) {
            if (t)
                mCore.IncreasePureReference(t->Locked);
            mGuard.Lock();
            list.push_back(t);
            mGuard.Unlock();
        }
        bool UpdateBuffer();
            
    };
    class StateDataDivider {
    public:
        //LockFreeQueue_ts2<mfxBitstream*> m_Bitstreams;
        std::list<mfxBitstream*> m_Bitstreams;
        std::auto_ptr<AudioFramesCollector> m_AudioFrames;
    protected:
        int m_nRestBytes;
        mfxU32 m_FrameSize;
        mfxStatus m_lastStatus;
        CommonCORE* m_CommonCore;

        int m_nFrames;

        std::vector<mfxU8> m_buffer;
        UMC::Mutex m_mGuard;

        void QueueBitstream(mfxBitstream * p) {
            m_mGuard.Lock();
            m_Bitstreams.push_back(p);
            m_mGuard.Unlock();
        }

        void QueueFrame(mfxAudioFrame * p) {
            m_AudioFrames->push(p);
            if (p) {
                m_nFrames   += (m_nRestBytes + p->DataLength) / m_FrameSize;
                m_nRestBytes = (m_nRestBytes + p->DataLength) % m_FrameSize;
            }
        }
        
        bool QueuePair(std::pair<mfxAudioFrame*, mfxBitstream*> pair) {
            if (MFX_ERR_MORE_BITSTREAM == m_lastStatus) {
                QueueBitstream(pair.second);
            } else if (MFX_ERR_MORE_DATA == m_lastStatus) {
                QueueFrame(pair.first);
                if (!pair.first)
                    return true;

            } else if (MFX_ERR_NONE == m_lastStatus) {
                QueueFrame(pair.first);
                QueueBitstream(pair.second);
            }
            return false;
        }
    public:
        StateDataDivider(int ExpectedFrameSize, CommonCORE* core) 
            : m_nRestBytes(0)
            , m_FrameSize(ExpectedFrameSize)
            , m_lastStatus(MFX_ERR_NONE)
            , m_CommonCore(core)
            , m_nFrames(0) {
                m_buffer.resize(ExpectedFrameSize);
                m_AudioFrames.reset(new AudioFramesCollector(m_buffer, *m_CommonCore));
        
        }
        mfxStatus PutPair(std::pair<mfxAudioFrame*, mfxBitstream*> pair) {
            bool bNullFrame = QueuePair(pair);
            if (bNullFrame) {
                if(!m_nRestBytes)
                    return MFX_ERR_MORE_DATA;
                m_nRestBytes = 0;
                return MFX_ERR_NONE;
            } else if (m_nFrames > 1) {
                m_nFrames--;
                return m_lastStatus = MFX_ERR_MORE_BITSTREAM;
            } else if (m_nFrames == 1) {
                m_lastStatus = m_nRestBytes ?  MFX_ERR_MORE_BITSTREAM : MFX_ERR_NONE;
                m_nFrames--;
                return m_lastStatus;
            } else {
                return m_lastStatus = MFX_ERR_MORE_DATA;
            } 
        }

        mfxBitstream* GetNextBitstream() {
            mfxBitstream *bitstream = 0;
            m_mGuard.Lock();
            if (!m_Bitstreams.empty()) {
                bitstream = m_Bitstreams.front();
                m_Bitstreams.pop_front();
            }
            m_mGuard.Unlock();
            return bitstream;
        }

        mfxU8* GetBuffer() {
            mfxU8* pBuffer = m_AudioFrames->UpdateBuffer() ? &m_buffer.front() : 0;
            return pBuffer;
        }
        
    };

    mfxU32 m_FrameSize;
    std::auto_ptr<StateDataDivider> m_divider;
};

#endif // __MFX_AAC_ENCODE_H__

#endif // MFX_ENABLE_AAC_AUDIO_ENCODE
