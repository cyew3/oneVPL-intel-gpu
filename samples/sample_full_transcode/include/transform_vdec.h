//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "itransform.h"
#include "isample.h"
#include "hw_device.h"
#include "samples_pool.h"
#include "base_allocator.h"
#include <iostream>
#include "video_session.h"

class PipelineFactory;

template <>
class Transform <MFXVideoDECODE> : public ITransform, private no_copy{
public:
    Transform(PipelineFactory& factory, MFXVideoSessionExt & session, int TimeToWait);
    virtual ~Transform(){}
    virtual void Configure(MFXAVParams& , ITransform *pNext) ;
    //take ownership over input sample
    virtual void PutSample( SamplePtr& sample);
    virtual bool GetSample( SamplePtr& sample);

    virtual void GetNumSurfaces(MFXAVParams& param, IAllocRequest& request) {
        mfxStatus sts = m_pDEC->QueryIOSurf(&param.GetVideoParam(), &request.Video());
        if (sts < 0) {
            MSDK_TRACE_ERROR(MSDK_STRING("m_pDEC->QueryIOSurf, sts=") << sts);
            throw DecodeQueryIOSurfaceError();
        }
    }

private:
    PipelineFactory& m_factory;
    MFXVideoSessionExt& m_session;
    ITransform* m_pNextTransform;
    int m_dTimeToWait;
    bool m_bDrainSamplefSent;
    bool m_bEOS;
    bool mBshouldLoad;

    SamplePtr m_pInput;
    std::auto_ptr<CHWDevice> m_pDevice;
    std::auto_ptr<SamplePool> m_pSamplesSurfPool;
    std::auto_ptr<BaseFrameAllocator> m_pAllocator;

    std::auto_ptr<MFXVideoDECODE> m_pDEC;
    mfxVideoParam m_decParam;
    void InitDecode(SamplePtr& sample);
    void AllocFrames(SamplePtr& sample);
    void CreateAllocatorAndSetHandle();
    void CreateAllocatorAndDevice(AllocatorImpl impl);
    //splitter complete frame WA
    std::vector<mfxU8> m_dataVec;
protected:
    bool    m_bInited;
    mfxU16  m_nFramesForNextTransform;
};
