//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
#include "transform_plugin.h"
#include "pipeline_factory.h"


Transform <MFXVideoUSER>::Transform(PipelineFactory& factory, MFXVideoSessionExt& session,
                                    mfxPluginType type, const msdk_string & pluginFullPath, int TimeToWait) : 
    m_factory(factory)
    ,m_session(session)
    ,m_PluginType(type)
    {
        pluginFullPath;
        TimeToWait;
        
}


void Transform <MFXVideoUSER>::Configure(MFXAVParams& /*param*/, ITransform *) {
   
}

bool Transform <MFXVideoUSER>::GetSample( SamplePtr& /*sample*/) {
    return false;
}


void Transform <MFXVideoUSER>::PutSample(SamplePtr& /*sample*/) {
   
}
