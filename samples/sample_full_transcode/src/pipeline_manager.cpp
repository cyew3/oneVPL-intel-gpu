/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include "pipeline_manager.h"
#include "exceptions.h"
#include <iostream>
#include "trace.h"

PipelineManager::PipelineManager(PipelineFactory &factory)
    : m_factory(factory)
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
    if (!session)
        return;
    std::auto_ptr<ITransform> dec(m_factory.CreateAudioDecoderTransform(*session, 60000));

    m_transforms->RegisterTransform(TransformConfigDesc(info.SID, aParam), dec);

    if (m_profile->isAudioEncoderExist((int)nTrack)) {
        std::auto_ptr<ITransform> enc(m_factory.CreateAudioEncoderTransform(*session, 60000));
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

    mfxVideoParam vParam = {};
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