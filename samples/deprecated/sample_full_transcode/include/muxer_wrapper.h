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

#include <memory>
#include "isample.h"
#include "mfxsplmux++.h"
#include "isink.h"
#include "exceptions.h"
#include <iostream>
#include <map>
#include "sample_metadata.h"
#include "stream_params_ext.h"
#include <list>

namespace detail {

    //accumulate input samples just before muxing them, in order to receive sps pps before initialize muxer
    class SamplePoolForMuxer {
        std::list<ISample*> m_samples;
        size_t m_nTracks;
        MetaData m_muxVideoParam;
        MetaData m_SpsPps;
    public:
        SamplePoolForMuxer(size_t nTracks) : m_nTracks(nTracks) {
        }
        virtual bool GetSample(SamplePtr& sample);
        virtual void PutSample(SamplePtr& sample);
        virtual bool GetVideoInfo(mfxTrackInfo& track_info);
    };
}


class MuxerWrapper : public ISink, private no_copy
{
protected:
    MFXStreamParamsExt m_sParamsExt;
    MFXStreamParams m_sParams;
    std::auto_ptr<MFXDataIO> m_io;
    std::auto_ptr<MFXMuxer>  m_muxer;
    bool               m_bInited;
    detail::SamplePoolForMuxer m_SamplePool;

public:
    MuxerWrapper(std::auto_ptr<MFXMuxer>& muxer, const MFXStreamParamsExt & sParams, std::auto_ptr<MFXDataIO>& io)
        : m_sParamsExt(sParams)
        , m_sParams(m_sParamsExt.GetMFXStreamParams())
        , m_io(io)
        , m_muxer(muxer)
        , m_bInited(false)
        , m_SamplePool(m_sParamsExt.Tracks().size()) {
    }
    ~MuxerWrapper() {
    }
    virtual void PutSample(SamplePtr & sample);
};
