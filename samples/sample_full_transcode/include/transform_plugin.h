//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "itransform.h"
#include <queue>
#include "mfxplugin++.h"

class PipelineFactory;


template <>
class Transform <MFXVideoUSER> : public ITransform, private no_copy {
public:
    Transform(PipelineFactory& /*factory*/, MFXVideoSessionExt& /*session*/, mfxPluginType /*type*/, const msdk_string & /*pluginFullPath*/, int /*TimeToWait*/);
    virtual void Configure(MFXAVParams& , ITransform *pNext) ;
    virtual void PutSample(SamplePtr&);
    virtual bool GetSample(SamplePtr&);
    virtual void GetNumSurfaces(MFXAVParams& , IAllocRequest& ){};
private:

    PipelineFactory& m_factory;
    MFXVideoSessionExt& m_session;
    std::auto_ptr<MFXVideoVPP> m_pVPP;
    std::auto_ptr<SamplePool> m_pSamplesSurfPool; 

    int  m_dTimeToWait;

    mfxFrameSurface1* m_pInputSurface;
    mfxPluginType               m_PluginType;

    mfxVideoParam m_initVideoParam;

    void InitVPP();
protected:
    bool m_bInited;
};
