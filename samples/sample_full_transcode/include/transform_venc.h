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
#include "samples_pool.h"
#include <iostream>
#include <vector>
#include "video_session.h"

class PipelineFactory;

template <>
class Transform <MFXVideoENCODE> : public ITransform, private no_copy{
public:
    Transform(PipelineFactory& factory, MFXVideoSessionExt & session, int TimeToWait);
    virtual ~Transform() {};
    virtual void Configure(MFXAVParams& , ITransform *pNext) ;
    //take ownership over input sample
    virtual void PutSample( SamplePtr& );
    virtual bool GetSample( SamplePtr& sample);

    virtual void GetNumSurfaces(MFXAVParams& param, IAllocRequest& request);

private:

    MFXVideoSessionExt& m_session;
    PipelineFactory& m_factory;
    int  m_dTimeToWait;
    bool m_bInited;
    bool m_bCreateSPS;
    bool m_bDrainSampleSent;
    bool m_bEOS;
    bool m_bDrainComplete;
    int m_nTrackId;


    std::queue<std::pair<ISample*, mfxSyncPoint> > m_ExtBitstreamQueue;

    std::auto_ptr<SamplePool> m_pSamplesSurfPool;
    std::auto_ptr<MFXVideoENCODE> m_pENC;
    std::auto_ptr<SamplePool> m_BitstreamPool;

    mfxVideoParam m_initVideoParam;

    void InitEncode(SamplePtr&);
    void MakeSPSPPS(std::vector<char>& );
    void UpdateInitVideoParams();
    void SubmitEncodingWorkload(mfxFrameSurface1 * surface);

    std::vector<mfxU8> m_Sps;
    std::vector<mfxU8> m_Pps;
    std::vector<char>  m_SpsPps;
};
