//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//

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
    std::auto_ptr<MFXMuxer>  m_muxer;
    std::auto_ptr<MFXDataIO> m_io;
    MFXStreamParamsExt m_sParamsExt;
    MFXStreamParams m_sParams;
    bool               m_bInited;
    detail::SamplePoolForMuxer m_SamplePool;

public:
    MuxerWrapper(std::auto_ptr<MFXMuxer>& muxer, const MFXStreamParamsExt & sParams, std::auto_ptr<MFXDataIO>& io) 
        : m_muxer(muxer)
        , m_io(io)
        , m_sParamsExt(sParams)
        , m_sParams(m_sParamsExt.GetMFXStreamParams())
        , m_bInited(false)
        , m_SamplePool(m_sParamsExt.Tracks().size()) {
    }
    ~MuxerWrapper() {
    }
    virtual void PutSample(SamplePtr & sample);
};
