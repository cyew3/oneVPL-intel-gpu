/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mfx_samples_config.h"

#include "pipeline_manager.h"
#include "exceptions.h"
#include <iostream>
#include "trace.h"

PipelineManager::PipelineManager(PipelineFactory &factory)
    : m_factory(factory)
    , m_bJustCopyAudio(false)
{
}

PipelineManager::~PipelineManager() {

}

void PipelineManager::PushTroughTransform(TransformStorage::iterator current, TransformStorage::iterator last) {
    if (current == last) {
        m_pSink->PutSample(m_WorkerSample);
        return;
    }

    (*current)->PutSample(m_WorkerSample);

    //pull cycle
    for (;;) {
        if (!(*current)->GetSample(m_WorkerSample)) {
            break;
        }
        PushTroughTransform(current + 1, last);
    }
}

void PipelineManager::Run() {
    for (;;) {
        if (!m_pSource->GetSample(m_WorkerSample)) {
            break;
        }
        //unexpected track
        //shouldn't happen due to splitter design, but if stream has dynamic tracks, or we disabled some tracks, will gracefully handle
        if (m_transforms->empty(m_WorkerSample->GetTrackID())) {
            if (m_newTracks.find(m_WorkerSample->GetTrackID()) == m_newTracks.end()) {
                MSDK_TRACE_INFO(MSDK_STRING("got sample from track: ")<<m_WorkerSample->GetTrackID()<<MSDK_STRING(", skipping"));
                m_newTracks[m_WorkerSample->GetTrackID()] = true;
            }
            continue;
        }
        if (msdk_trace_is_printable(MSDK_TRACE_LEVEL_INFO)) {
            msdk_printf(MSDK_CHAR("."));
        }
        //push through transform filters chain
        PushTroughTransform(m_transforms->begin(m_WorkerSample->GetTrackID())
                           ,m_transforms->end(m_WorkerSample->GetTrackID()));

    }
}

void PipelineManager::Build(CmdLineParser &parser) {
    //setting-up splitter
    std::auto_ptr<MFXDataIO> input (m_factory.CreateFileIO(parser[OPTION_I].as<msdk_string>(), MSDK_STRING("rb")));

    if (!parser.IsPresent(OPTION_LOOP)) {
        m_pSource.reset(m_factory.CreateSplitterWrapper(input));
    } else {
        m_pSource.reset(m_factory.CreateCircularSplitterWrapper(input, parser[OPTION_LOOP].as<mfxU64>()));
    }

    if (parser.IsPresent(OPTION_ACODEC_COPY))
        m_bJustCopyAudio = true;

    MFXStreamParams sp;
    m_pSource->GetInfo(sp);

    //profile holds all pipeline configuration from commandline and from streamparams
    m_profile.reset( m_factory.CreatePipelineProfile(sp, parser));

    MSDK_TRACE_INFO(MSDK_STRING("Input     : ") << TraceStreamName(parser[OPTION_I].as<msdk_string>()));
    MSDK_TRACE_INFO(MSDK_STRING("NumTracks : ") << sp.NumTracks);
    MSDK_TRACE_INFO(MSDK_STRING("Output    : ") << TraceStreamName(parser[OPTION_O].as<msdk_string>()));

    MFXSessionInfo ainfo = m_profile->isAudioDecoderExist(0) ? m_profile->GetAudioTrackInfo(0).Session : MFXSessionInfo();
    MFXSessionInfo vinfo = m_profile->isDecoderExist() ? m_profile->GetVideoTrackInfo().Session : MFXSessionInfo();

    m_sessions.reset(m_factory.CreateSessionStorage( vinfo, ainfo));
    m_transforms.reset(m_factory.CreateTransformStorage());

    //setting up chains
    for (int i = 0; m_profile->isAudioDecoderExist(i); i++) {
        BuildAudioChain(i);
    }

    BuildVideoChain();
    m_transforms->Resolve();
    if (m_profile->isMultiplexerExist()) {
        m_StreamParams = m_profile->GetMuxerInfo();


        std::auto_ptr<MFXDataIO> dataio(m_factory.CreateFileIO(parser[OPTION_O].as<msdk_string>(),  MSDK_STRING("wb")));
        m_pSink.reset(m_factory.CreateMuxerWrapper(m_StreamParams, dataio));
    } else {
        m_pSink.reset(m_factory.CreateFileIOWrapper(parser[OPTION_O].as<msdk_string>(),  MSDK_STRING("wb")));
    }
}


void PipelineManager::BuildAudioChain(size_t nTrack) {

    AudioTrackInfoExt info = m_profile->GetAudioTrackInfo(nTrack);
    //TODO: add audio support
    mfxAudioParam aParam = {};
    aParam = info.Decode;

    MFXAudioSession *session = m_sessions->GetAudioSessionForID(info.SID);
    if (!session && !m_bJustCopyAudio)
        return;
    std::auto_ptr<ITransform> dec;

    m_bJustCopyAudio ? dec.reset(m_factory.CreateAudioDecoderNullTransform(*session, 60000))
        : dec.reset(m_factory.CreateAudioDecoderTransform(*session, 60000));


    m_transforms->RegisterTransform(TransformConfigDesc(info.SID, aParam), dec);

    if (m_profile->isAudioEncoderExist((int)nTrack)) {
        std::auto_ptr<ITransform> enc;
        m_bJustCopyAudio ? enc.reset(m_factory.CreateAudioEncoderNullTransform(*session, 60000))
            : enc.reset(m_factory.CreateAudioEncoderTransform(*session, 60000));

        aParam = info.Encode;
        m_transforms->RegisterTransform(TransformConfigDesc(info.SID, aParam), enc);
    }
}

void PipelineManager::BuildVideoChain()
{
    if (!m_profile->isDecoderExist()) {
        return;
    }

    VideoTrackInfoExt info = m_profile->GetVideoTrackInfo();
    MFXVideoSessionExt &session = *m_sessions->GetVideoSessionForID(info.SID);

    mfxVideoParam vParam;
    MSDK_ZERO_MEMORY(vParam);
    vParam = info.Decode;

    //starting components creation
    std::auto_ptr<ITransform> dec(m_factory.CreateVideoDecoderTransform(session, 60000));
    m_transforms->RegisterTransform(TransformConfigDesc(info.SID, vParam), dec );

    if (m_profile->isGenericPluginExist()) {
        //std::auto_ptr<ITransform> plg(m_factory.CreatePluginTransform(m_profile->(session, 60000));
    }

    if (m_profile->isVppExist()) {
        std::auto_ptr<ITransform> vpp(m_factory.CreateVideoVPPTransform(session, 60000));
        m_transforms->RegisterTransform(TransformConfigDesc(info.SID, vParam), vpp);
    }

    if (m_profile->isEncoderExist()) {
        std::auto_ptr<ITransform> enc(m_factory.CreateVideoEncoderTransform(session, 60000));
        vParam = info.Encode;
        m_transforms->RegisterTransform(TransformConfigDesc(info.SID, vParam), enc);
    }
}