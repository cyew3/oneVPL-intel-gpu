//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "itransform.h"
#include <queue>
#include "base_allocator.h"

class PipelineFactory;

template <>
class Transform<MFXVideoVPP> : public ITransform, private no_copy {
public:
    Transform(PipelineFactory& , MFXVideoSessionExt &, int );
    virtual void Configure(MFXAVParams& , ITransform *pNext);
    virtual void PutSample(SamplePtr&);
    virtual bool GetSample(SamplePtr&);
    virtual void GetNumSurfaces(MFXAVParams& param, IAllocRequest& request);
private:

    MFXVideoSessionExt& m_session;
    PipelineFactory& m_factory;
    int  m_dTimeToWait;
    std::auto_ptr<MFXVideoVPP> m_pVPP;
    mfxFrameSurface1* m_pInputSurface;
    std::auto_ptr<SamplePool>  m_pSamplesSurfPool; 

    std::queue<ISample*> m_ExtBitstreamQueue;

    std::vector<mfxFrameSurface1> m_SurfArray;
    mfxVideoParam m_initVideoParam;
    void InitVPP();
    void AllocFrames();
protected:
    bool   m_bInited;
    mfxU16 m_nFramesForNextTransform;
};
