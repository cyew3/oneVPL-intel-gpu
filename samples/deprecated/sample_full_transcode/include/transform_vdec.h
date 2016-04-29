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
        if ((sts < 0) || (request.Video().NumFrameSuggested < param.GetVideoParam().AsyncDepth)) {
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
    std::auto_ptr<SamplePool> m_pSamplesSurfPool;

    std::auto_ptr<MFXVideoDECODE> m_pDEC;
    mfxVideoParam m_decParam;
    void InitDecode(SamplePtr& sample);
    void AllocFrames(SamplePtr& sample);

    //splitter complete frame WA
    std::vector<mfxU8> m_dataVec;
protected:
    bool    m_bInited;
    mfxU16  m_nFramesForNextTransform;
};
