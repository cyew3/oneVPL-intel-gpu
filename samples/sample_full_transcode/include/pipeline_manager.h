//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
#pragma once

#include <sample_utils.h>
#include <list>
#include "pipeline_factory.h"
#include "plugin_utils.h"

class PipelineManager  : private no_copy
{
protected:
    PipelineFactory &m_factory;
    std::auto_ptr<SessionStorage> m_sessions;
    std::auto_ptr<ISplitterWrapper> m_pSource;
    std::auto_ptr<ISink> m_pSink;
    std::auto_ptr<TransformStorage> m_transforms;
    SamplePtr m_WorkerSample;
    std::auto_ptr<IPipelineProfile> m_profile;
    std::map<int, bool> m_newTracks;
    MFXStreamParamsExt m_StreamParams;
    bool m_bJustCopyAudio;

public:
    PipelineManager(PipelineFactory &factory);
    virtual ~PipelineManager();

    virtual void Run();
    virtual void Build(CmdLineParser &parser);

protected:
    virtual void PushTroughTransform(TransformStorage::iterator current, TransformStorage::iterator last);
    virtual void BuildAudioChain(size_t track);
    virtual void BuildVideoChain();
};

